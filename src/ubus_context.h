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

#include <libutype/avl.h>
#include <blobpack/blobpack.h>

#define UBUS_MAX_NOTIFY_PEERS	16

#include "ubus_message.h"
#include "ubus_request.h"
#include "ubus_method.h"
#include "ubus_object.h"
#include "ubus_srv.h"
#include "ubus_proxy.h"
#include "ubus_context.h"

#define UBUS_DEFAULT_SOCKET "/var/run/ubus.sock"

struct ubus_context {
	struct avl_tree peers_by_id;
	struct avl_tree peers_by_name;

	//struct avl_tree objects_by_name; 
	//struct avl_tree objects_by_id; 
	struct ubus_object *root_obj; 

	struct list_head requests;
	struct list_head pending; 
	struct list_head pending_incoming; 

	struct ubus_socket *socket; 

	uint16_t request_seq;

	struct blob buf; 

	char *name; // connection name for this context

	void *user_data; 
};

struct ubus_context *ubus_new(const char *name, struct ubus_object **root);
void ubus_delete(struct ubus_context **self); 
int ubus_connect(struct ubus_context *self, const char *path, uint32_t *peer_id); 
int ubus_listen(struct ubus_context *self, const char *path); 

int ubus_set_peer_localname(struct ubus_context *self, uint32_t peer, const char *localname); 

int ubus_send_request(struct ubus_context *self, struct ubus_request **req); 
//uint32_t ubus_add_object(struct ubus_context *self, struct ubus_object **obj); 
int ubus_handle_events(struct ubus_context *self); 

const char *ubus_status_to_string(int8_t status); 

static inline void ubus_set_userdata(struct ubus_context *self, void *ptr){ self->user_data = ptr; }
static inline void *ubus_get_userdata(struct ubus_context *self) { return self->user_data; }

