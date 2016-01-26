/*
 * Copyright (C) 2011-2015 
 * 		Felix Fietkau <nbd@openwrt.org>
 * 		ubus2: Martin Schröder <mkschreder.uk@gmail.com>
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
#include <libutype/list.h>
#include <libusys/uloop.h>
#include <blobpack/blobpack.h>
#include <stdint.h>

#include "ubus_context.h"
#include "ubus_srv.h"
#include "ubus_server.h"
#include "ubus_client.h"
#include "ubus_srv_ws.h"

bool url_scanf(const char *url, char *proto, char *host, int *port, char *path); 
