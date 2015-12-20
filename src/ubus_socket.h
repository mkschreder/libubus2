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

#ifndef __LIBUBUS_IO_H
#define __LIBUBUS_IO_H

//extern struct blob_buf b;
//extern const struct ubus_method watch_method;

#include "ubus_message.h"

#include <libutype/list.h>
#include <libusys/uloop.h>

#include "ubus_id.h"

struct ubus_pending_msg {
	struct list_head list;
	struct ubus_msghdr_buf hdr;
};

#define UBUS_UNIX_SOCKET "/var/run/ubus.sock"

struct ubus_context; 
struct ubus_request; 

struct ubus_socket; 

typedef void (*ubus_socket_data_cb_t)(struct ubus_socket *self, uint32_t peer, uint8_t type, uint32_t serial, struct blob_attr *msg);  
typedef void (*ubus_socket_client_cb_t)(struct ubus_socket *self, uint32_t peer);  

struct ubus_socket {
	struct avl_tree clients; 

	int listen_fd;

	struct blob_buf buf; 

	ubus_socket_data_cb_t on_message; 
	ubus_socket_client_cb_t on_client_connected; 

	//struct list_head clients; 
	//struct list_head requests;
	//struct list_head pending;

	//struct ubus_msghdr_buf msgbuf;
	//struct uloop_fd sock;
	//struct uloop_timeout pending_timer;

	//uint32_t msgbuf_data_len;
	//int msgbuf_reduction_counter;


}; 


struct ubus_socket *ubus_socket_new(void); 
void ubus_socket_delete(struct ubus_socket **self); 

void ubus_socket_init(struct ubus_socket *self); 
void ubus_socket_destroy(struct ubus_socket *self); 

int ubus_socket_listen(struct ubus_socket *self, const char *path); 
int ubus_socket_connect(struct ubus_socket *self, const char *path);

int ubus_socket_send(struct ubus_socket *self, uint32_t peer, int type, struct blob_attr *msg); 
static inline void ubus_socket_on_message(struct ubus_socket *self, ubus_socket_data_cb_t cb){
	self->on_message = cb; 
}

void  ubus_socket_poll(struct ubus_socket *self, int timeout); 
//struct blob_attr **ubus_parse_msg(struct blob_attr *msg);
/*void ubus_handle_data(struct uloop_fd *u, unsigned int events);
int ubus_send_msg(struct ubus_context *ctx, uint32_t seq,
		  void *msg, size_t size, int cmd, uint32_t peer, int fd);
void ubus_process_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, int fd);
int __hidden ubus_start_request(struct ubus_context *ctx, struct ubus_request *req,
				void *msg, size_t size, int cmd, uint32_t peer);
void ubus_process_obj_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, struct blob_attr **attrbuf);
void ubus_process_req_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, int fd);
void  ubus_poll_data(struct ubus_context *ctx, int timeout); 
*/

#endif
