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

#define __packetdata __attribute__((packed)) __attribute__((__aligned__(4)))

#define UBUS_MSG_CHUNK_SIZE	65536

#define UBUS_SYSTEM_OBJECT_EVENT	1
#define UBUS_SYSTEM_OBJECT_MAX		1024

struct ubus_msghdr {
	uint8_t version;
	uint8_t type;
	uint16_t seq;
	uint32_t peer;
} __packetdata;

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

	__UBUS_MSG_LAST
}; 

static __attribute__((unused)) const char *ubus_message_types[] = {
	"UBUS_MSG_HELLO",
	"UBUS_MSG_METHOD_CALL", 
	"UBUS_MSG_METHOD_RETURN",
	"UBUS_MSG_ERROR", 
	"UBUS_MSG_SIGNAL"
}; 

enum ubus_msg_attr {
	UBUS_ATTR_UNSPEC,

	UBUS_ATTR_STATUS,

	UBUS_ATTR_OBJPATH,
	UBUS_ATTR_OBJID,
	UBUS_ATTR_METHOD,

	UBUS_ATTR_OBJTYPE,
	UBUS_ATTR_SIGNATURE,

	UBUS_ATTR_DATA,
	UBUS_ATTR_TARGET,

	UBUS_ATTR_ACTIVE,
	UBUS_ATTR_NO_REPLY,

	UBUS_ATTR_SUBSCRIBERS,

	/* must be last */
	UBUS_ATTR_MAX,
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

struct ubus_msghdr_buf {
	struct ubus_msghdr hdr;
	struct blob_attr *data;
};

void ubus_message_parse(int type, struct blob_attr *msg, struct blob_attr **attrbuf); 

#define UBUS_MAX_MSGLEN 10240000

#endif
