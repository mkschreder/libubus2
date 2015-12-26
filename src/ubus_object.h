/*
 * Copyright (C) 2015 Martin Schröder <mkschreder.uk@gmail.com>
 * Copyright (C) 2011-2014 Felix Fietkau <nbd@openwrt.org>
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

#include <inttypes.h>
#include <libutype/avl.h>

struct blob_buf; 
struct ubus_context; 
struct ubus_method; 

struct ubus_object {
	struct avl_node avl;
	char *name;

	struct list_head methods; 

	void *priv; // private data to attach to owner of the object 
};

struct ubus_object *ubus_object_new(const char *name); 
void ubus_object_delete(struct ubus_object **obj); 

void ubus_object_init(struct ubus_object *obj, const char *name); 
void ubus_object_destroy(struct ubus_object *obj);

void ubus_object_add_method(struct ubus_object *obj, struct ubus_method **method); 
struct ubus_method *ubus_object_find_method(struct ubus_object *obj, const char *name); 
void ubus_object_publish_method(struct ubus_object *obj, struct ubus_method **method); 

static inline void ubus_object_set_userdata(struct ubus_object *self, void *ptr) { self->priv = ptr; }

void ubus_object_serialize(struct ubus_object *obj, struct blob_buf *buf); 



