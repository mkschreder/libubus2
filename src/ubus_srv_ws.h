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

#include <blobpack/blobpack.h>
#include "ubus_srv.h"

ubus_server_t ubus_srv_ws_new(const char *www_root); 

/*void json_websocket_delete(struct json_websocket **self); 

int json_websocket_listen(struct json_websocket *self, const char *path); 
int json_websocket_connect(struct json_websocket *self, const char *path); 

int json_websocket_handle_events(struct json_websocket *self); 

static inline void json_websocket_on_message(struct json_websocket *self, json_websocket_data_cb_t cb){ self->on_message = cb; } 
*/
