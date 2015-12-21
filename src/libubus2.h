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
#include "ubus_directory.h"
#include "ubus_socket.h"

struct ubus_context;
struct ubus_msg_src;
struct ubus_object;
struct ubus_request;
struct ubus_request_data;
struct ubus_object_data;
struct ubus_event_handler;
struct ubus_subscriber;
struct ubus_notify_request;

#define UBUS_OBJECT_TYPE(_name, _methods)		\
	{						\
		.name = _name,				\
		.id = 0,				\
		.n_methods = ARRAY_SIZE(_methods),	\
		.methods = _methods			\
	}

#define __UBUS_METHOD_NOARG(_name, _handler)		\
	.name = _name,					\
	.handler = _handler

#define __UBUS_METHOD(_name, _handler, _policy)		\
	__UBUS_METHOD_NOARG(_name, _handler),		\
	.policy = _policy,				\
	.n_policy = ARRAY_SIZE(_policy)

#define UBUS_METHOD(_name, _handler, _policy)		\
	{ __UBUS_METHOD(_name, _handler, _policy) }

#define UBUS_METHOD_MASK(_name, _handler, _policy, _mask) \
	{						\
		__UBUS_METHOD(_name, _handler, _policy),\
		.mask = _mask				\
	}

#define UBUS_METHOD_NOARG(_name, _handler)		\
	{ __UBUS_METHOD_NOARG(_name, _handler) }

