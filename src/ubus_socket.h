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

#pragma once

#include "ubus_message.h"

#include <libutype/list.h>
#include <libusys/uloop.h>

#include "ubus_id.h"

#define UBUS_PEER_BROADCAST (-1)

struct ubus_context; 
struct ubus_request; 

struct ubus_socket; 

typedef void (*ubus_socket_data_cb_t)(struct ubus_socket *self, uint32_t peer, uint8_t type, uint32_t serial, struct blob_attr *msg);  
typedef void (*ubus_socket_client_cb_t)(struct ubus_socket *self, uint32_t peer);  

struct ubus_socket {
	struct avl_tree clients; 

	int listen_fd;

	struct blob_buf buf; 

	ubus_socket_data_cb_t on_message; 
	ubus_socket_client_cb_t on_client_connected; 

	void *user_data; 
}; 


struct ubus_socket *ubus_socket_new(void); 
void ubus_socket_delete(struct ubus_socket **self); 

void ubus_socket_init(struct ubus_socket *self); 
void ubus_socket_destroy(struct ubus_socket *self); 

int ubus_socket_listen(struct ubus_socket *self, const char *path); 
int ubus_socket_connect(struct ubus_socket *self, const char *path);

int ubus_socket_send(struct ubus_socket *self, int32_t peer, int type, uint16_t serial, struct blob_attr *msg); 
static inline void ubus_socket_on_message(struct ubus_socket *self, ubus_socket_data_cb_t cb){
	self->on_message = cb; 
}
static inline void ubus_socket_on_client_connected(struct ubus_socket *self, ubus_socket_client_cb_t cb){
	self->on_client_connected = cb; 
}

void  ubus_socket_poll(struct ubus_socket *self, int timeout); 

static inline void ubus_socket_set_userdata(struct ubus_socket *self, void *ptr){
	self->user_data = ptr; 
}

