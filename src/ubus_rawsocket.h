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

#include "ubus_socket.h"
#include "ubus_message.h"

#include <libutype/list.h>

#include "ubus_id.h"

#define UBUS_PEER_BROADCAST (-1)

struct ubus_context; 
struct ubus_request; 

struct ubus_rawsocket; 

//typedef void (*ubus_rawsocket_data_cb_t)(struct ubus_rawsocket *self, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg);  
//typedef void (*ubus_rawsocket_client_cb_t)(struct ubus_rawsocket *self, uint32_t peer);  

ubus_socket_t ubus_rawsocket_new(void); 
/*void ubus_rawsocket_delete(struct ubus_rawsocket **self); 

void ubus_rawsocket_init(struct ubus_rawsocket *self); 
void ubus_rawsocket_destroy(struct ubus_rawsocket *self); 

int ubus_rawsocket_listen(struct ubus_rawsocket *self, const char *path); 
int ubus_rawsocket_connect(struct ubus_rawsocket *self, const char *path, uint32_t *id);


int ubus_rawsocket_send(struct ubus_rawsocket *self, int32_t peer, int type, uint16_t serial, struct blob_field *msg); 
static inline void ubus_rawsocket_on_message(struct ubus_rawsocket *self, ubus_rawsocket_data_cb_t cb){
	self->on_message = cb; 
}
static inline void ubus_rawsocket_on_client_connected(struct ubus_rawsocket *self, ubus_rawsocket_client_cb_t cb){
	self->on_client_connected = cb; 
}

void  ubus_rawsocket_poll(struct ubus_rawsocket *self, int timeout); 

static inline void ubus_rawsocket_set_userdata(struct ubus_rawsocket *self, void *ptr){
	self->user_data = ptr; 
}
*/
