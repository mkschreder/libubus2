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
#include "ubus_rawsocket.h"

#define STATIC_IOV(_var) { .iov_base = (char *) &(_var), .iov_len = sizeof(_var) }

#define UBUS_MSGBUF_REDUCTION_INTERVAL	16

/**
Raw socket handles binary framed messages and guarantees full message transmission and delivery. 
**/

struct ubus_rawsocket {
	int fd; 

	struct list_head tx_queue; 

	// receive state 
	int recv_count;  
	struct ubus_msg_header hdr; 
	struct ubus_message *msg; 

	bool disconnected; 

	void *user_data; 

	const struct ubus_socket_api *api; 
}; 

struct ubus_msg_header {
	uint8_t hdr_size; 	// works as a magic. Must always be sizeof(struct ubus_msg_header)
	uint16_t crc; 		// sum of the data portion
	uint32_t data_size;	// length of the data that follows 
} __attribute__((packed)) __attribute__((__aligned__(4))); 

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

struct ubus_rawsocket_frame *ubus_rawsocket_frame_new(struct blob_field *msg){
	assert(msg); 
	struct ubus_rawsocket_frame *self = calloc(1, sizeof(struct ubus_rawsocket_frame)); 
	blob_init(&self->data, (char*)msg, blob_field_raw_pad_len(msg)); 
	INIT_LIST_HEAD(&self->list); 
	self->hdr.crc = _crc16((char*)msg, blob_field_raw_len(msg)); 
	self->hdr.data_size = blob_field_raw_pad_len(msg); 
	return self; 
}

void ubus_rawsocket_frame_delete(struct ubus_rawsocket_frame **self){
	blob_free(&(*self)->data); 
	free(*self); 
	*self = NULL; 
}

void _rawsocket_destroy(ubus_socket_t socket){
	struct ubus_rawsocket *self = container_of(socket, struct ubus_rawsocket, api); 
	if(self->fd > 0) close(self->fd); 
	free(self); 
}

int _ubus_rawsocket_recv(struct ubus_rawsocket *self, struct ubus_message **msg){
	// if we still have not received the header
	if(!self->fd == -1) return -1; 

	if(self->recv_count < sizeof(struct ubus_msg_header)){
		int rc = recv(self->fd, ((char*)&self->hdr) + self->recv_count, sizeof(struct ubus_msg_header) - self->recv_count, 0); 
		if(rc > 0){
			self->recv_count += rc; 
		} else if(rc == 0){
			printf("recv disconnected!\n"); 
			close(self->fd); 
			self->fd = -1; 
			return -1; 
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
			if(cursor == self->hdr.data_size) break; 
		}
		if(rc == 0){
			close(self->fd); 	
			self->fd = -1; 
			return -1; 
		}
		// if we have received the full message then we call the message callback
		if(self->recv_count == (sizeof(struct ubus_msg_header) + self->hdr.data_size)){
			struct blob_field *msg = blob_head(&self->data); 
			if(blob_field_data_len(msg) > 0 && self->hdr.crc != _crc16((char*)msg, blob_field_raw_len(msg))){
				fprintf(stderr, "CRC mismatch!\n"); 
				//blob_field_dump_json(msg); 
				close(self->fd); 
				self->fd = -1; 
				return -1;  
			} else {
				*msg = self->msg; 
				
			}
			self->recv_count = 0; 
		}
	}
	return -EAGAIN; 
}

void _ubus_rawsocket_client_send(struct ubus_rawsocket_client *self){
	if(list_empty(&self->tx_queue)){
		return; 
	}

	struct ubus_rawsocket_frame *req = list_first_entry(&self->tx_queue, struct ubus_rawsocket_frame, list);
	if(req->send_count < sizeof(struct ubus_msg_header)){
		int sc = send(self->fd, ((char*)&req->hdr) + req->send_count, sizeof(struct ubus_msg_header) - req->send_count, MSG_NOSIGNAL); 
		if(sc > 0){
			req->send_count += sc; 
		}
		if(sc == 0) {
			self->disconnected = true; 
			return; 
		}
	}
	if(req->send_count >= sizeof(struct ubus_msg_header)){
		int cursor = req->send_count - sizeof(struct ubus_msg_header); 
		int sc; 
		int buf_size = blob_field_raw_pad_len(blob_head(&req->data)); 
		while((sc = send(self->fd, blob_head(&req->data) + cursor, buf_size - cursor, MSG_NOSIGNAL)) > 0){
			req->send_count += sc; 
			cursor = req->send_count - sizeof(struct ubus_msg_header);
			if(cursor == buf_size) break; 
		}
		if(sc == 0) {
			self->disconnected = true; 
			return; 
		}

		if(req->send_count == (sizeof(struct ubus_msg_header) + buf_size)){
			// full buffer was transmitted so we destroy the request
			list_del_init(&req->list); 
			//printf("removed completed request from queue! %d bytes\n", req->send_count); 
			ubus_rawsocket_frame_delete(&req); 
		}
	}
}

int ubus_rawsocket_send(ubus_socket_t socket, struct blob_field *msg){
	struct ubus_rawsocket *self = container_of(socket, struct ubus_rawsocket, api); 

	list_add(&req->list, &self->tx_queue); 
	return 0; 	
}

static void _rawsocket_on_message(ubus_socket_t socket, ubus_socket_msg_cb_t cb){
	struct ubus_rawsocket *self = container_of(socket, struct ubus_rawsocket, api); 
	self->on_message = cb; 
}

static void *_rawsocket_userdata(ubus_socket_t socket, void *ptr){
	struct ubus_rawsocket *self = container_of(socket, struct ubus_rawsocket, api); 
	if(!ptr) return self->user_data; 
	self->user_data = ptr; 
	return ptr; 
}

ubus_socket_t ubus_rawsocket_new(void){
	struct ubus_rawsocket *self = calloc(1, sizeof(struct ubus_rawsocket)); 
	// virtual api 
	static const struct ubus_socket_api api = {
		.destroy = _rawsocket_destroy, 
		.listen = _rawsocket_listen, 
		.connect = _rawsocket_connect, 
		.disconnect = _rawsocket_disconnect, 
		.send = _rawsocket_send, 
		.handle_events = _rawsocket_handle_events, 
		.on_message = _rawsocket_on_message, 
		.userdata = _rawsocket_userdata
	}; 
	self->api = &api; 
	return &self->api; 
}
