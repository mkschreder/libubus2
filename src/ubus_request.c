/*
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

#include <unistd.h>
#include "libubus2.h"

struct ubus_request *ubus_request_new(void){
	struct ubus_request *req = calloc(1, sizeof(struct ubus_request)); 
	return req; 
}

void ubus_request_delete(struct ubus_request **self){
	free(*self); 
	*self = 0; 
}


