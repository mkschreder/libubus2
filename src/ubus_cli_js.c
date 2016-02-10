/*
 * Copyright (C) 2015,2016 Martin Schröder <mkschreder.uk@gmail.com>
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
#include "ubus_id.h"

#include <blobpack/blobpack.h>

#include "ubus_cli.h"
#include "ubus_cli_js.h"
#include "ubus_message.h"
#include <assert.h>

#define STATIC_IOV(_var) { .iov_base = (char *) &(_var), .iov_len = sizeof(_var) }

#define UBUS_MSGBUF_REDUCTION_INTERVAL	16


struct ubus_json_frame {
	struct list_head list; 

	char *data; 
	int data_size; 
	int send_count; 
}; 

struct ubus_cli_js {
	struct list_head tx_queue; 
	struct list_head rx_queue; 
	int fd; 

	char *recv_buffer; 
	int recv_size; 
	int recv_count; 
	
	struct ubus_message *msg; 
	const struct ubus_client_api *api; 

	void *user_data; 
}; 

struct ubus_json_frame *ubus_json_frame_new(struct blob_field *msg){
	assert(msg); 
	struct ubus_json_frame *self = calloc(1, sizeof(struct ubus_json_frame)); 
	char *json = blob_field_to_json(msg); 
	self->data_size = strlen(json) + 1; 
	self->data = calloc(1, self->data_size + 1); 
	sprintf(self->data, "%s\n", json); 
	//printf("new frame %s\n", self->data); 
	free(json); 
	INIT_LIST_HEAD(&self->list); 
	return self; 
}

void ubus_json_frame_delete(struct ubus_json_frame **self){
	free((*self)->data); 
	free(*self); 
	*self = NULL; 
}

static void ubus_cli_js_init(struct ubus_cli_js *self){
	INIT_LIST_HEAD(&self->tx_queue); 
	INIT_LIST_HEAD(&self->rx_queue); 
	self->fd = -1; 
	self->recv_size = 16535; 
	self->recv_buffer = calloc(1, self->recv_size); 
	self->recv_count = 0; 
	self->msg = ubus_message_new(); 
}

static void _ubus_cli_js_destroy(ubus_client_t cl){
	struct ubus_cli_js *self = container_of(cl, struct ubus_cli_js, api); 
	shutdown((self)->fd, SHUT_RDWR); 
	close((self)->fd); 
	ubus_message_delete(&self->msg); 
	free((self)->recv_buffer); 
	free(self); 
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

static int _ubus_cli_js_connect(ubus_client_t socket, const char *_address){
	struct ubus_cli_js *self = container_of(socket, struct ubus_cli_js, api); 
	int flags = 0; 
	int addrlen = strlen(_address); 
	char *address = alloca(addrlen); 
	strcpy(address, _address); 
	char *port = NULL; 
	if(address[0] == '/' || address[0] == '.') flags |= USOCK_UNIX; 
	else _split_address_port(address, addrlen, &port); 
	int fd = usock(flags, address, port);
	if (fd < 0)
		return -1;

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

	self->fd = fd; 

	return 0; 
}

static int _ubus_cli_js_recv(ubus_client_t client, struct ubus_message **msg){
	struct ubus_cli_js *self = container_of(client, struct ubus_cli_js, api); 

	if(!list_empty(&self->rx_queue)){
		*msg = list_first_entry(&self->rx_queue, struct ubus_message, list); 
		list_del_init(&(*msg)->list); 
		return 1; 
	}

	int rc = recv(self->fd, self->recv_buffer + self->recv_count, self->recv_size - self->recv_count, 0); 
	if(rc == 0){
		return 0; 
	}

	if(rc > 0){
		// check if we got a new line (message separator)
		self->recv_count += rc;

		// check if buffer full
		if(self->recv_count == self->recv_size){
			//TODO: handle properly or resize the buffer 
			printf("too large message!\n"); 
			close(self->fd); 
		}

		// process as many messages as we can and save any extra data in the recv buffer 
		while(true){
			char *ch; 
			for(ch = self->recv_buffer; 
				*ch && *ch != '\n' && ch < (self->recv_buffer + self->recv_count); ch++){
			}
			if(*ch == '\n'){
				*ch = 0; 
				blob_reset(&self->msg->buf); 
				if(blob_put_json(&self->msg->buf, self->recv_buffer)){
					//printf("json data received!\n"); 
					list_add_tail(&self->msg->list, &self->rx_queue); 
					self->msg = ubus_message_new();  
				}

				int pos = (ch - self->recv_buffer + 1); 
				int rest_size = self->recv_count - pos; 
				//printf("rest size %d\n", rest_size); 
				if(rest_size > 0){
					char *rest = calloc(1, self->recv_size); 
					memcpy(rest, self->recv_buffer + pos, rest_size); 
					free(self->recv_buffer); 
					self->recv_buffer = rest; 
					self->recv_count = rest_size; 
					continue; 
				} else {
					self->recv_count = 0; 
				}
			} 
			break; 
		}
	} 
	if(list_empty(&self->rx_queue)) return rc; 
	*msg = list_first_entry(&self->rx_queue, struct ubus_message, list); 
	list_del_init(&(*msg)->list); 
	return 1; 
}

static void _ubus_client_send(struct ubus_cli_js *self){
	if(list_empty(&self->tx_queue)){
		return; 
	}

	struct ubus_json_frame *req = list_first_entry(&self->tx_queue, struct ubus_json_frame, list);

	int sc; 
	while((sc = send(self->fd, req->data + req->send_count, req->data_size - req->send_count, MSG_NOSIGNAL)) > 0){
		req->send_count += sc; 
	}

	// TODO: handle disconnect
	if(req->send_count == req->data_size){
		printf("sent frame %s\n", req->data); 
		// full buffer was transmitted so we destroy the request
		list_del_init(&req->list); 
		//printf("removed completed request from queue! %d bytes\n", req->send_count); 
		ubus_json_frame_delete(&req); 
	} else {
		printf("has not sent frame!\n"); 
	}
}

static int _ubus_cli_js_send(ubus_client_t socket, struct ubus_message **msg){
	struct ubus_cli_js *self = container_of(socket, struct ubus_cli_js, api); 
	
	struct ubus_json_frame *req = ubus_json_frame_new(blob_field_first_child(blob_head(&(*msg)->buf)));
	list_add_tail(&req->list, &self->tx_queue); 
	
	_ubus_client_send(self); 

	ubus_message_delete(msg); 

	return 0; 	
}

static void *_ubus_cli_js_userdata(ubus_client_t socket, void *ptr){
	struct ubus_cli_js *self = container_of(socket, struct ubus_cli_js, api); 
	if(!ptr) return self->user_data; 
	self->user_data = ptr; 
	return ptr; 
}

static int _ubus_cli_js_disconnect(ubus_client_t socket){
	struct ubus_cli_js *self = container_of(socket, struct ubus_cli_js, api); 
	printf("client disconnected!\n"); 
	if(self->fd > 0) close(self->fd); 
	self->fd = -1; 
	return 0; 
}

ubus_client_t ubus_cli_js_new(void){
	struct ubus_cli_js *self = calloc(1, sizeof(struct ubus_cli_js)); 
	ubus_cli_js_init(self); 
	static const struct ubus_client_api api = {
		.destroy = _ubus_cli_js_destroy, 
		.connect = _ubus_cli_js_connect, 
		.disconnect = _ubus_cli_js_disconnect, 
		.send = _ubus_cli_js_send, 
		.recv = _ubus_cli_js_recv, 
		.userdata = _ubus_cli_js_userdata
	}; 
	self->api = &api; 
	return &self->api; 
}
