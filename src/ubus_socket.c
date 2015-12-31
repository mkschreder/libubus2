/*
 * Copyright (C) 2015 Martin Schröder <mkschreder.uk@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#ifdef FreeBSD
#include <sys/param.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <libusys/usock.h>

#include <blobpack/blobpack.h>

#include "ubus_socket.h"

#define STATIC_IOV(_var) { .iov_base = (char *) &(_var), .iov_len = sizeof(_var) }

#define UBUS_MSGBUF_REDUCTION_INTERVAL	16

struct ubus_msg_header {
	uint8_t hdr_size; 	// works as a magic. Must always be sizeof(struct ubus_msg_header)
	uint8_t type;  		// type of request 
	uint16_t seq;		// request sequence that is set by sender 
	uint16_t crc; 		// sum of the data portion
	uint32_t data_size;	// length of the data that follows 
} __attribute__((packed)) __attribute__((__aligned__(4))); 

struct ubus_frame {
	struct list_head list; 
	struct ubus_msg_header hdr; 
	struct blob data; 

	int send_count; 
}; 

struct ubus_client {
	struct ubus_id id; 
	struct list_head tx_queue; 
	int fd; 

	int recv_count; 
	struct ubus_msg_header hdr; 
	struct blob data; 
	//struct list_head rx_queue;
}; 

uint16_t _crc16(char* data_p, uint32_t length){
    unsigned char x;
    uint16_t crc = 0xFFFF;

    while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

static struct ubus_client *ubus_client_new(int fd){
	struct ubus_client *self = calloc(1, sizeof(struct ubus_client)); 
	INIT_LIST_HEAD(&self->tx_queue); 
	self->fd = fd; 
	self->recv_count = 0; 
	blob_init(&self->data, 0, 0); 
	return self; 
}

static void ubus_client_delete(struct ubus_client **self){
	shutdown((*self)->fd, SHUT_RDWR); 
	close((*self)->fd); 
	blob_free(&(*self)->data); 
	free(*self); 
	*self = NULL;
}

struct ubus_frame *ubus_frame_new(int type, uint16_t seq, struct blob_field *msg){
	assert(msg); 
	struct ubus_frame *self = calloc(1, sizeof(struct ubus_frame)); 
	blob_init(&self->data, (char*)msg, blob_field_raw_pad_len(msg)); 
	INIT_LIST_HEAD(&self->list); 
	self->hdr.type = type;  
	self->hdr.seq = seq; 
	self->hdr.crc = _crc16((char*)msg, blob_field_raw_len(msg)); 
	//self->hdr.data_size = cpu_to_be32((uint32_t)blob_field_pad_len(msg)); 
	//printf("new frame size %d, be: %d\n", blob_field_pad_len(msg), cpu_to_be32(blob_field_pad_len(msg))); 
	self->hdr.data_size = blob_field_raw_pad_len(msg); 
	return self; 
}

void ubus_frame_delete(struct ubus_frame **self){
	blob_free(&(*self)->data); 
	free(*self); 
	*self = NULL; 
}

struct ubus_socket *ubus_socket_new(void){
	struct ubus_socket *self = calloc(1, sizeof(struct ubus_socket)); 
	ubus_socket_init(self); 
	return self; 
}

void ubus_socket_delete(struct ubus_socket **self){
	ubus_socket_destroy(*self); 
	free(*self); 
	*self = NULL; 
}

void ubus_socket_init(struct ubus_socket *self){
	//INIT_LIST_HEAD(&self->clients); 
	ubus_id_tree_init(&self->clients); 
}

void ubus_socket_destroy(struct ubus_socket *self){
	struct ubus_id *id, *tmp; 
	avl_for_each_element_safe(&self->clients, id, avl, tmp){
		struct ubus_client *client = container_of(id, struct ubus_client, id);  
		ubus_id_free(&self->clients, &client->id); 
		ubus_client_delete(&client); 
	}
	if(self->listen_fd) close(self->listen_fd);
}

static void _accept_connection(struct ubus_socket *self){
	bool done = false; 

	do {
		int client = accept(self->listen_fd, NULL, 0);
		if ( client < 0 ) {
			switch (errno) {
			case ECONNABORTED:
			case EINTR:
				done = true;
			default:
				return;  
			}
		}

		// configure client into non blocking mode
		fcntl(client, F_SETFL, fcntl(client, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

		struct ubus_client *cl = ubus_client_new(client); 
		ubus_id_alloc(&self->clients, &cl->id, 0); 
		
		if(self->on_client_connected){
			self->on_client_connected(self, cl->id.id); 
		}
	} while (!done);
}

static void _split_address_port(char *address, int addrlen, char **port){
	for(int c = 0; c < addrlen; c++){
		if(address[c] == ':' && c != (addrlen - 1)) {
			address[c] = 0; 
			*port = address + c + 1; 
			break; 
		}
	}
}

int ubus_socket_listen(struct ubus_socket *self, const char *_address){
	assert(_address);
	int addrlen = strlen(_address); 
	char *address = alloca(addrlen); 
	strcpy(address, _address); 
	char *port = NULL; 
	int flags =  USOCK_SERVER | USOCK_NONBLOCK; 
	if(address[0] == '/' || address[0] == '.'){
		umask(0177);
		unlink(address);
		flags |= USOCK_UNIX; 
	} else {
		_split_address_port(address, addrlen, &port); 
	}
	printf("trying to listen on %s %s\n", address, port); 
	self->listen_fd = usock(flags, address, port);
	if (self->listen_fd < 0) {
		perror("usock");
		return -1; 
	}
	return 0; 
}

int ubus_socket_connect(struct ubus_socket *self, const char *_address, uint32_t *id){
	int flags = 0; 
	int addrlen = strlen(_address); 
	char *address = alloca(addrlen); 
	strcpy(address, _address); 
	char *port = NULL; 
	if(address[0] == '/' || address[0] == '.') flags |= USOCK_UNIX; 
	else _split_address_port(address, addrlen, &port); 
	int fd = usock(flags, address, port);
	if (fd < 0)
		return -UBUS_STATUS_CONNECTION_FAILED;

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

	struct ubus_client *cl = ubus_client_new(fd); 
	ubus_id_alloc(&self->clients, &cl->id, 0); 
	
	// connecting out generates the same event as connecting in
	if(self->on_client_connected){
		self->on_client_connected(self, cl->id.id); 
	}

	if(id) *id = cl->id.id; 

	return 0; 
}

void _ubus_client_recv(struct ubus_client *self, struct ubus_socket *socket){
	// if we still have not received the header
	if(self->recv_count < sizeof(struct ubus_msg_header)){
		int rc = recv(self->fd, ((char*)&self->hdr) + self->recv_count, sizeof(struct ubus_msg_header) - self->recv_count, 0); 
		if(rc > 0){
			self->recv_count += rc; 
		} 
	}
	// if we have just received the header then we allocate the body based on size in the header
	if(self->recv_count == sizeof(struct ubus_msg_header)){
		// TODO: validate header here!

		//printf("got frame size %d, correct: %d\n", self->hdr.data_size, be32_to_cpu(self->hdr.data_size)); 
		//self->hdr.data_size = be32_to_cpu(self->hdr.data_size); 
		//printf("receiving message of %d bytes\n", self->hdr.data_size); 
		if(self->hdr.data_size <= 0){
			//fprintf(stderr, "protocol error! null message!\n"); 
			// fail with assertion failure
			assert(self->hdr.data_size > 0); 
		}
		blob_resize(&self->data, self->hdr.data_size); 
	}
	// if we have received the header then we receive the body here
	if(self->recv_count >= sizeof(struct ubus_msg_header)){
		int rc = 0; 
		int cursor = self->recv_count - sizeof(struct ubus_msg_header); 
		while((rc = recv(self->fd, (char*)(blob_head(&self->data)) + cursor, self->hdr.data_size - cursor, 0)) > 0){
			self->recv_count += rc; 
			cursor = self->recv_count - sizeof(struct ubus_msg_header);
		}
		// if we have received the full message then we call the message callback
		if(self->recv_count == (sizeof(struct ubus_msg_header) + self->hdr.data_size)){
			struct blob_field *msg = blob_head(&self->data); 
			if(blob_field_data_len(msg) > 0 && self->hdr.crc != _crc16((char*)msg, blob_field_raw_len(msg))){
				fprintf(stderr, "CRC mismatch!\n"); 
				//blob_field_dump_json(msg); 
				close(self->fd); 
				return;  
			} else {
				if(socket->on_message){
					socket->on_message(socket, self->id.id, self->hdr.type, self->hdr.seq, msg); 
				}
			}
			self->recv_count = 0; 
		}
	}
}

void _ubus_client_send(struct ubus_client *self){
	if(list_empty(&self->tx_queue)){
		return; 
	}

	struct ubus_frame *req = list_first_entry(&self->tx_queue, struct ubus_frame, list);
	if(req->send_count < sizeof(struct ubus_msg_header)){
		int sc = send(self->fd, ((char*)&req->hdr) + req->send_count, sizeof(struct ubus_msg_header) - req->send_count, MSG_NOSIGNAL); 
		if(sc > 0){
			req->send_count += sc; 
		}
		// TODO: handle disconnect
	}
	if(req->send_count >= sizeof(struct ubus_msg_header)){
		int cursor = req->send_count - sizeof(struct ubus_msg_header); 
		int sc; 
		int buf_size = blob_field_raw_pad_len(blob_head(&req->data)); 
		while((sc = send(self->fd, blob_head(&req->data) + cursor, buf_size - cursor, MSG_NOSIGNAL)) > 0){
			req->send_count += sc; 
			cursor = req->send_count - sizeof(struct ubus_msg_header);
		}
		// TODO: handle disconnect
		if(req->send_count == (sizeof(struct ubus_msg_header) + buf_size)){
			// full buffer was transmitted so we destroy the request
			list_del_init(&req->list); 
			//printf("removed completed request from queue! %d bytes\n", req->send_count); 
			ubus_frame_delete(&req); 
		}
	}
}

void ubus_socket_poll(struct ubus_socket *self, int timeout){
	int count = avl_size(&self->clients) + 1; 
	struct pollfd *pfd = alloca(sizeof(struct pollfd) * count); 
	memset(pfd, 0, sizeof(struct pollfd) * count); 
	struct ubus_client **clients = alloca(sizeof(void*)*100); 
	pfd[0] = (struct pollfd){ .fd = self->listen_fd, .events = POLLIN | POLLERR }; 
	clients[0] = 0; 

	int c = 1; 
	struct ubus_id *id;  
	avl_for_each_element(&self->clients, id, avl){
		struct ubus_client *client = (struct ubus_client*)container_of(id, struct ubus_client, id);  
		pfd[c] = (struct pollfd){ .fd = client->fd, .events = POLLOUT | POLLIN | POLLERR };  
		clients[c] = client;  
		c++; 
	}		
	
	// try to send more data
	for(int c = 1; c < count; c++){
		_ubus_client_send(clients[c]); 
	}

	int ret = 0; 
	if((ret = poll(pfd, count, timeout)) > 0){
		if(pfd[0].revents != 0){
			// TODO: check for errors
			_accept_connection(self);
		}
		for(int c = 1; c < count; c++){
			if(pfd[c].revents != 0){
				if(pfd[c].revents & POLLHUP || pfd[c].revents & POLLRDHUP){
					//printf("ERROR: peer hung up!\n"); 
					_ubus_client_recv(clients[c], self); 
					avl_delete(&self->clients, &clients[c]->id.avl); 
					ubus_client_delete(&clients[c]); 
					continue; 
				} else if(pfd[c].revents & POLLERR){
					printf("ERROR: socket error!\n"); 
				} else if(pfd[c].revents & POLLIN) {
					// receive as much data as we can
					_ubus_client_recv(clients[c], self); 
				}
			}
		}
	}
}

int ubus_socket_send(struct ubus_socket *self, int32_t peer, int type, uint16_t serial, struct blob_field *msg){
	struct ubus_id *id;  
	if(peer == UBUS_PEER_BROADCAST){
		avl_for_each_element(&self->clients, id, avl){
			struct ubus_client *client = (struct ubus_client*)container_of(id, struct ubus_client, id);  
			struct ubus_frame *req = ubus_frame_new(type, serial, msg);
			list_add(&req->list, &client->tx_queue); 
			// try to send as much as we can right away
			_ubus_client_send(client); 
			//printf("added request to tx_queue!\n"); 
		}		
	} else {
		struct ubus_id *id = ubus_id_find(&self->clients, peer); 
		if(!id) return -1; 
		struct ubus_client *client = (struct ubus_client*)container_of(id, struct ubus_client, id);  
		struct ubus_frame *req = ubus_frame_new(type, serial, msg);
		list_add(&req->list, &client->tx_queue); 
		_ubus_client_send(client); 
	}
	return 0; 	
}

