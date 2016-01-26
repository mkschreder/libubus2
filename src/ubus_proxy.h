/*
 * Copyright (C) 2016 Martin K. Schröder <mkschreder.uk@gmail.com>
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

#include "ubus_id.h"
#include "ubus_srv.h"

struct ubus_proxy {
	struct avl_tree clients_in; 
	struct avl_tree clients_out; 
	struct ubus_socket *insock; 
	struct ubus_socket *outsock; 
	char *outpath; 
}; 

struct ubus_proxy *ubus_proxy_new(struct ubus_socket **insock, struct ubus_socket **outsock); 
void ubus_proxy_delete(struct ubus_proxy **self); 

int ubus_proxy_handle_events(struct ubus_proxy *self); 

int ubus_proxy_listen(struct ubus_proxy *self, const char *path); 
int ubus_proxy_connect(struct ubus_proxy *self, const char *path); 

