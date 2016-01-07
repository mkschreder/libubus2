/*
 * Copyright (C) 2011 Felix Fietkau <nbd@openwrt.org>
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

#ifndef __UBUSMSG_H
#define __UBUSMSG_H

#include <stdint.h>
#include <blobpack/blobpack.h>

enum ubus_msg_type {
	// initial server message
	UBUS_MSG_HELLO,

	// method call
	UBUS_MSG_METHOD_CALL, 

	// method success return 
	UBUS_MSG_METHOD_RETURN, 

	// request failure 
	UBUS_MSG_ERROR, 

	// asynchronous signal message
	UBUS_MSG_SIGNAL,

	/** APPLICATION MESSAGES (ONLY for use by library to signal callback. Never over network!) **/
	UBUS_MSG_PEER_CONNECTED,
	UBUS_MSG_PEER_DISCONNECTED, 
	__UBUS_MSG_LAST
}; 

static __attribute__((unused)) const char *ubus_message_types[] = {
	"UBUS_MSG_HELLO",
	"UBUS_MSG_METHOD_CALL", 
	"UBUS_MSG_METHOD_RETURN",
	"UBUS_MSG_ERROR", 
	"UBUS_MSG_SIGNAL"
}; 

enum ubus_msg_status {
	UBUS_STATUS_OK,
	UBUS_STATUS_INVALID_COMMAND,
	UBUS_STATUS_INVALID_ARGUMENT,
	UBUS_STATUS_METHOD_NOT_FOUND,
	UBUS_STATUS_NOT_FOUND,
	UBUS_STATUS_NO_DATA,
	UBUS_STATUS_PERMISSION_DENIED,
	UBUS_STATUS_TIMEOUT,
	UBUS_STATUS_NOT_SUPPORTED,
	UBUS_STATUS_UNKNOWN_ERROR,
	UBUS_STATUS_CONNECTION_FAILED,
	__UBUS_STATUS_LAST
};

#endif
