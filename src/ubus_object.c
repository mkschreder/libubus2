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
	self->avl.key = self->name; 
	INIT_LIST_HEAD(&self->methods); 
}

void ubus_object_destroy(struct ubus_object *self){
	struct ubus_method *m, *tmp; 
	list_for_each_entry_safe(m, tmp, &self->methods, list){
		ubus_method_delete(&m); 
	}
	free(self->name); 
}

void ubus_object_add_method(struct ubus_object *self, struct ubus_method **method){
	list_add(&(*method)->list, &self->methods); 	
	*method = NULL; 
}

struct ubus_method *ubus_object_find_method(struct ubus_object *self, const char *name){
	struct ubus_method *m; 
	list_for_each_entry(m, &self->methods, list){
		if(strcmp(m->name, name) == 0) return m; 
	}
	return NULL; 
}

void ubus_object_serialize(struct ubus_object *self, struct blob *buf){
	struct list_head *pos = NULL; 
	blob_offset_t ofs = blob_open_table(buf); 
	list_for_each(pos, &self->methods){
		struct ubus_method *m = container_of(pos, struct ubus_method, list); 
		blob_put_string(buf, m->name); 
		blob_put_attr(buf, blob_head(&m->signature)); 	
	}
	blob_close_table(buf, ofs); 
}

