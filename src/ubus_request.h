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

#include <blobpack/blobpack.h>
#include <libusys/uloop_timeout.h>

struct ubus_request; 

typedef void (*ubus_request_cb_t)(struct ubus_request *req, struct blob_field *msg); 

struct ubus_request {
	struct list_head list; 
	char *object; 
	char *method; 
	struct blob buf; 
	uint16_t seq; 

	uint32_t src_id; 
	char *dst_name; 
	uint32_t dst_id; 

	bool resolved; 
	bool failed; 
	
	utick_t timeout; 

	ubus_request_cb_t on_resolve; 
	ubus_request_cb_t on_fail; 

	void *user_data; 
}; 

#define UBUS_LOCAL_BUS (NULL)
struct ubus_request *ubus_request_new(const char *client, const char *object, const char *method, struct blob_field *msg); 
void ubus_request_delete(struct ubus_request **self); 

static inline void ubus_request_set_userdata(struct ubus_request *self, void *ptr){
	self->user_data = ptr; 
}

static inline void* ubus_request_get_userdata(struct ubus_request *self) {
	return self->user_data; 
}

void ubus_request_resolve(struct ubus_request *self, struct blob_field *msg); 
void ubus_request_reject(struct ubus_request *self, struct blob_field *msg); 

static inline void ubus_request_on_resolve(struct ubus_request *self, ubus_request_cb_t cb){
	self->on_resolve = cb; 
}

static inline void ubus_request_on_reject(struct ubus_request *self, ubus_request_cb_t cb){
	self->on_fail = cb; 
}

