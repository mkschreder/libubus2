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

#include "libubus2.h"
#include "ubus_request.h"
#include <blobpack/blobpack.h>

struct ubus_request *ubus_request_new(const char *client, const char *object, const char *method, struct blob_field *msg){
	struct ubus_request *self = calloc(1, sizeof(struct ubus_request)); 
	self->dst_name = strdup(client); 
	self->object = strdup(object); 
	self->method = strdup(method); 
	blob_init(&self->buf, 0, 0); 	
	blob_put_attr(&self->buf, msg); 
	return self; 
}

void ubus_request_delete(struct ubus_request **_self){
	struct ubus_request *self = *_self; 
	free(self->dst_name); 
	free(self->object); 
	free(self->method); 
	blob_free(&self->buf); 
	free(self); 
	*_self = NULL; 
}

void ubus_request_resolve(struct ubus_request *self, struct blob_field *msg){
	// TODO: decide whether we should have it like this
	if(!msg){
		blob_reset(&self->buf); 
		msg = blob_head(&self->buf); 
	}
	if(self->on_resolve) self->on_resolve(self, msg); 
	self->resolved = true; 
}

void ubus_request_reject(struct ubus_request *self, struct blob_field *msg){
	// TODO: decide whether we should have it like this
	if(!msg){
		blob_reset(&self->buf); 
		msg = blob_head(&self->buf); 
	}
	if(self->on_fail) self->on_fail(self, msg); 
	self->failed = true; 
}

