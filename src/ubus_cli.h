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

#include <inttypes.h>
#include "ubus_message.h"

#define UBUS_PEER_BROADCAST (-1)

struct ubus_client_api; 
typedef const struct ubus_client_api** ubus_client_t; 

struct ubus_client_api {
	void 	(*destroy)(ubus_client_t ptr); 
	int 	(*connect)(ubus_client_t ptr, const char *path);
	int 	(*disconnect)(ubus_client_t ptr);
	int 	(*send)(ubus_client_t ptr, struct ubus_message **msg); 
	int 	(*recv)(ubus_client_t ptr, struct ubus_message **msg); 
	void*	(*userdata)(ubus_client_t ptr, void *data); 
}; 

#define UBUS_TARGET_PEER (0)
#define UBUS_BROADCAST_PEER (-1)

#define ubus_client_delete(sock) {(*sock)->destroy(sock); sock = NULL;} 
#define ubus_client_connect(sock, path) (*sock)->connect(sock, path) 
#define ubus_client_send(sock, msg) (*sock)->send(sock, msg)
#define ubus_client_recv(sock, msg) (*sock)->recv(sock, msg)
#define ubus_client_get_userdata(sock) (*sock)->userdata(sock, NULL)
#define ubus_client_set_userdata(sock, ptr) (*sock)->userdata(sock, ptr)
