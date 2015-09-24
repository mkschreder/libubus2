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

#ifndef __LIBUBUS_H
#define __LIBUBUS_H

#include <libubox/avl.h>
#include <libubox/list.h>
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <stdint.h>

#define UBUS_MAX_MSGLEN 10240000
#define UBUS_UNIX_SOCKET "/var/run/ubus.sock"
#define UBUS_MAX_NOTIFY_PEERS	16

struct ubus_context;
struct ubus_msg_src;
struct ubus_object;
struct ubus_request;
struct ubus_request_data;
struct ubus_object_data;
struct ubus_event_handler;
struct ubus_subscriber;
struct ubus_notify_request;

#include "ubus_message.h"
#include "ubus_request.h"
#include "ubus_method.h"
#include "ubus_object_type.h"
#include "ubus_object.h"
#include "ubus_socket.h"
#include "ubus_subscriber.h"
#include "ubus_context.h"

typedef void (*ubus_event_handler_t)(struct ubus_context *ctx, struct ubus_event_handler *ev,
				     const char *type, struct blob_attr *msg);
typedef void (*ubus_notify_complete_handler_t)(struct ubus_notify_request *req,
					       int idx, int ret);
typedef void (*ubus_connect_handler_t)(struct ubus_context *ctx);

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

struct ubus_event_handler {
	struct ubus_object obj;

	ubus_event_handler_t cb;
};
struct ubus_notify_request {
	struct ubus_request req;

	ubus_notify_complete_handler_t status_cb;
	ubus_notify_complete_handler_t complete_cb;

	uint32_t pending;
	uint32_t id[UBUS_MAX_NOTIFY_PEERS + 1];
};

struct ubus_auto_conn {
	struct ubus_context ctx;
	struct uloop_timeout timer;
	const char *path;
	ubus_connect_handler_t cb;
};

struct ubus_context *ubus_connect(const char *path);
int ubus_connect_ctx(struct ubus_context *ctx, const char *path);
void ubus_auto_connect(struct ubus_auto_conn *conn);
int ubus_reconnect(struct ubus_context *ctx, const char *path);

/* call this only for struct ubus_context pointers returned by ubus_connect() */
void ubus_free(struct ubus_context *ctx);

/* call this only for struct ubus_context pointers initialised by ubus_connect_ctx() */
void ubus_shutdown(struct ubus_context *ctx);

const char *ubus_strerror(int error);

static inline void ubus_add_uloop(struct ubus_context *ctx)
{
	uloop_fd_add(&ctx->sock, ULOOP_BLOCKING | ULOOP_READ);
}

/* call this for read events on ctx->sock.fd when not using uloop */
static inline void ubus_handle_event(struct ubus_context *ctx)
{
	ctx->sock.cb(&ctx->sock, ULOOP_READ);
}

/* ----------- raw request handling ----------- */

/* wait for a request to complete and return its status */
int ubus_complete_request(struct ubus_context *ctx, struct ubus_request *req,
			  int timeout);

/* complete a request asynchronously */
void ubus_complete_request_async(struct ubus_context *ctx,
				 struct ubus_request *req);

/* abort an asynchronous request */
void ubus_abort_request(struct ubus_context *ctx, struct ubus_request *req);

/* ----------- objects ----------- */

int ubus_lookup(struct ubus_context *ctx, const char *path,
		ubus_lookup_handler_t cb, void *priv);

int ubus_lookup_id(struct ubus_context *ctx, const char *path, uint32_t *id);

/* make an object visible to remote connections */
int ubus_add_object(struct ubus_context *ctx, struct ubus_object *obj);

/* remove the object from the ubus connection */
int ubus_remove_object(struct ubus_context *ctx, struct ubus_object *obj);

/* add a subscriber notifications from another object */
int ubus_register_subscriber(struct ubus_context *ctx, struct ubus_subscriber *obj);

static inline int
ubus_unregister_subscriber(struct ubus_context *ctx, struct ubus_subscriber *obj)
{
	return ubus_remove_object(ctx, &obj->obj);
}

int ubus_subscribe(struct ubus_context *ctx, struct ubus_subscriber *obj, uint32_t id);
int ubus_unsubscribe(struct ubus_context *ctx, struct ubus_subscriber *obj, uint32_t id);

/* ----------- rpc ----------- */

/* invoke a method on a specific object */
int ubus_invoke(struct ubus_context *ctx, uint32_t obj, const char *method,
		struct blob_attr *msg, ubus_data_handler_t cb, void *priv,
		int timeout);

/* asynchronous version of ubus_invoke() */
int ubus_invoke_async(struct ubus_context *ctx, uint32_t obj, const char *method,
		      struct blob_attr *msg, struct ubus_request *req);

/* send a reply to an incoming object method call */
int ubus_send_reply(struct ubus_context *ctx, struct ubus_request_data *req,
		    struct blob_attr *msg);

static inline void ubus_defer_request(struct ubus_context *ctx,
				      struct ubus_request_data *req,
				      struct ubus_request_data *new_req)
{
    memcpy(new_req, req, sizeof(*req));
    req->deferred = true;
}

static inline void ubus_request_set_fd(struct ubus_context *ctx,
				       struct ubus_request_data *req, int fd)
{
    req->fd = fd;
}

void ubus_complete_deferred_request(struct ubus_context *ctx,
				    struct ubus_request_data *req, int ret);

/*
 * send a notification to all subscribers of an object
 * if timeout < 0, no reply is expected from subscribers
 */
int ubus_notify(struct ubus_context *ctx, struct ubus_object *obj,
		const char *type, struct blob_attr *msg, int timeout);

int ubus_notify_async(struct ubus_context *ctx, struct ubus_object *obj,
		      const char *type, struct blob_attr *msg,
		      struct ubus_notify_request *req);


/* ----------- events ----------- */

int ubus_send_event(struct ubus_context *ctx, const char *id,
		    struct blob_attr *data);

int ubus_register_event_handler(struct ubus_context *ctx,
				struct ubus_event_handler *ev,
				const char *pattern);

static inline int ubus_unregister_event_handler(struct ubus_context *ctx,
						struct ubus_event_handler *ev)
{
    return ubus_remove_object(ctx, &ev->obj);
}

#endif
