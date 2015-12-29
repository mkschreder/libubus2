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


#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include "ubus_method.h"

#include <string.h>

struct ubus_method* ubus_method_new(const char *name, ubus_method_handler_t cb){
	struct ubus_method *self = calloc(1, sizeof(struct ubus_method)); 
	ubus_method_init(self, name, cb); 
	return self; 
}

void ubus_method_delete(struct ubus_method **self){
	ubus_method_destroy(*self); 
	free(*self); 
	*self = NULL; 
}

void ubus_method_init(struct ubus_method *self, const char *name, ubus_method_handler_t cb){
	if(name) self->name = strdup(name); 
	else self->name = 0; 
	self->handler = cb; 
	blob_init(&self->signature, 0, 0); 
}

void ubus_method_destroy(struct ubus_method *self){
	if(self->name) free(self->name); 
	blob_free(&self->signature); 
	self->handler = 0; 
}

void ubus_method_add_param(struct ubus_method *self, const char *name, const char *signature){
	blob_offset_t ofs = blob_open_array(&self->signature); 
		blob_put_int(&self->signature, UBUS_METHOD_PARAM_IN); 
		blob_put_string(&self->signature, name); 
		blob_put_string(&self->signature, signature); 
	blob_close_array(&self->signature, ofs); 
}

void ubus_method_add_return(struct ubus_method *self, const char *name, const char *signature){
	blob_offset_t ofs = blob_open_array(&self->signature); 
		blob_put_int(&self->signature, UBUS_METHOD_PARAM_OUT); 
		blob_put_string(&self->signature, name); 
		blob_put_string(&self->signature, signature); 
	blob_close_array(&self->signature, ofs); 
}

