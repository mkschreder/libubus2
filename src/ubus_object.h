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

#include <libutype/avl.h>

struct ubus_context; 
struct ubus_method; 

struct ubus_object {
	struct avl_node avl;

	char *name;
	uint32_t id;

	//char *path;
	//struct ubus_object_type *type;

	void (*subscribe_cb)(struct ubus_context *ctx, struct ubus_object *obj);
	bool has_subscribers;

	struct list_head methods; 

	//struct ubus_method *methods;
	//int n_methods;

	void *priv; // private data to attach to owner of the object 
};

struct ubus_object_data {
	uint32_t id;
	uint32_t type_id;
	uint32_t client_id; 
	const char *path;
	struct blob_attr *signature;
};

struct ubus_object *ubus_object_new(const char *name); 
void ubus_object_delete(struct ubus_object **obj); 

void ubus_object_init(struct ubus_object *obj, const char *name); 
void ubus_object_destroy(struct ubus_object *obj);

void ubus_object_add_method(struct ubus_object *obj, struct ubus_method **method); 
void ubus_object_publish_method(struct ubus_object *obj, struct ubus_method **method); 

typedef void (*ubus_lookup_handler_t)(struct ubus_context *ctx,
				      struct ubus_object_data *obj,
				      void *priv);


