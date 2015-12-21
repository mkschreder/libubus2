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

struct ubus_request; 

typedef void (*ubus_request_resolve_cb_t)(struct ubus_request *req, struct blob_attr *msg); 
typedef void (*ubus_request_fail_cb_t)(struct ubus_request *req, int code, struct blob_attr *msg); 

struct ubus_request {
	struct list_head list; 
	char *client; 
	char *object; 
	char *method; 
	struct blob_buf buf; 

	ubus_request_resolve_cb_t on_resolve; 
	ubus_request_fail_cb_t on_fail; 
}; 

struct ubus_request *ubus_request_new(const char *client, const char *object, const char *method, struct blob_attr *msg); 
void ubus_request_delete(struct ubus_request **self); 

static inline void ubus_request_on_resolve(struct ubus_request *self, ubus_request_resolve_cb_t cb){
	self->on_resolve = cb; 
}

static inline void ubus_request_on_fail(struct ubus_request *self, ubus_request_fail_cb_t cb){
	self->on_fail = cb; 
}

