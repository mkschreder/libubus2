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

#include "ubus_object.h"
#include "ubus_request.h"

#define UBUS_METHOD_PARAM_IN 1
#define UBUS_METHOD_PARAM_OUT 2

struct blob_attr; 
/*
typedef int (*ubus_request_handler_t)(struct ubus_context *ctx, struct ubus_object *obj,
			      struct ubus_request *req,
			      const char *method, struct blob_attr *msg);
*/
typedef int (*ubus_method_handler_t)(struct ubus_method *self, 
	struct ubus_context *ctx, 
	struct ubus_object *obj, 
	struct ubus_request *req);

struct ubus_method {
	char *name;
	ubus_method_handler_t handler;
	struct blob_buf signature; 
	
	// list head for the list of methods (TODO: maybe use avl for this?) 
	struct list_head list; 

	unsigned long mask;
	const struct blob_attr_policy *policy;
	int n_policy;
};

struct ubus_method *ubus_method_new(const char *name, ubus_method_handler_t cb);  
void ubus_method_delete(struct ubus_method **self); 

//! Add a parameter to list of parameters for the method. 
void ubus_method_add_param(struct ubus_method *self, const char *name, const char *signature); 
//! Add a return value to list of return values
void ubus_method_add_return(struct ubus_method *self, const char *name, const char *signature); 

void ubus_method_init(struct ubus_method *self, const char *name, ubus_method_handler_t cb); 
void ubus_method_destroy(struct ubus_method *self); 

static inline int ubus_method_invoke(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req){
	if(self->handler) return self->handler(self, ctx, obj, req); 
	return 0; 
}
