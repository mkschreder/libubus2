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

#include <libutype/avl-cmp.h>

#include "ubus_peer.h"

struct ubus_peer *ubus_peer_new(const char *key, uint32_t id){
	struct ubus_peer *self = calloc(1, sizeof(struct ubus_peer)); 
	self->name = strdup(key); 
	self->id = id; 
	self->avl_name.key = self->name; 
	self->avl_id.key = &self->id; 
	avl_init(&self->objects, avl_strcmp, false, NULL); 
	return self; 
}

void ubus_peer_delete(struct ubus_peer **_self){
	struct ubus_peer *self = *_self; 
	free(self->name); 
}

void ubus_peer_set_name(struct ubus_peer *self, const char *name){
	free(self->name); 
	self->name = strdup(name); 
	self->avl_name.key = self->name; 
}

int ubus_peer_add_object(struct ubus_peer *self, struct ubus_object **_obj){
	struct ubus_object *obj = *_obj;	
	if(avl_insert(&self->objects, &obj->avl) != 0) {
		ubus_object_delete(&obj); 
		*_obj = NULL; 
		return -1; 
	}
	return 0; 
}
