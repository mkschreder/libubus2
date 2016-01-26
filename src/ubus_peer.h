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

struct ubus_peer {
	struct avl_node avl_id; 
	struct avl_node avl_name; 
	struct avl_tree objects; 
	char *name; 
	uint32_t id; 
}; 

struct ubus_peer *ubus_peer_new(const char *name, uint32_t id); 
void ubus_peer_delete(struct ubus_peer **self); 

int ubus_peer_add_object(struct ubus_peer *self, struct ubus_object **obj); 
void ubus_peer_set_name(struct ubus_peer *self, const char *name); 

int ubus_peer_add_child_peer(struct ubus_peer *self, const char *name, uint32_t id); 
struct ubus_peer *ubus_peer_find_child_peer(struct ubus_peer *self, const char *name); 
