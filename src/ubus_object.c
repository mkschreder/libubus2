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

#include "ubus_context.h"

struct ubus_object *ubus_object_new(const char *name){
	struct ubus_object *self = malloc(sizeof(struct ubus_object)); 
	ubus_object_init(self, name); 
	return self; 
}

void ubus_object_delete(struct ubus_object **self){
	ubus_object_destroy(*self); 
	free(*self); 
	*self = NULL; 
}

void ubus_object_init(struct ubus_object *self, const char *name){
	memset(self, 0, sizeof(*self)); 
	self->name = strdup(name); 
	INIT_LIST_HEAD(&self->methods); 
}

void ubus_object_destroy(struct ubus_object *self){
	free(self->name); 
}

void ubus_object_add_method(struct ubus_object *self, struct ubus_method **method){
	list_add(&(*method)->list, &self->methods); 	
	*method = NULL; 
}

