/*
 * Copyright (C) 2011-2012 Felix Fietkau <nbd@openwrt.org>
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

#include "ubus_context.h"
#include "ubus_subscriber.h"

static int ubus_subscriber_cb(struct ubus_context *ctx, struct ubus_object *obj,
			 struct ubus_request_data *req,
			 const char *method, struct blob_attr *msg)
{
	struct ubus_subscriber *s;

	s = container_of(obj, struct ubus_subscriber, obj);
	if (s->cb)
		return s->cb(ctx, obj, req, method, msg);
	return 0;
}

int ubus_register_subscriber(struct ubus_context *ctx, struct ubus_subscriber *s)
{
	struct ubus_object *obj = &s->obj;
	
	struct ubus_method *watch = calloc(1, sizeof(struct ubus_method)); 
	ubus_method_init(watch, NULL, ubus_subscriber_cb); 

	s->watch_method = watch; 
	//obj->methods = watch;
	//obj->n_methods = 1;

	return ubus_add_object(ctx, obj);
}

int ubus_unregister_subscriber(struct ubus_context *ctx, struct ubus_subscriber *obj){
	return ubus_remove_object(ctx, &obj->obj);
}
/*
static int
__ubus_subscribe_request(struct ubus_context *ctx, struct ubus_object *obj, uint32_t id, int type){
	struct ubus_request req;

	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, obj->id);
	blob_buf_put_i32(&ctx->buf, id);

	if (ubus_start_request(ctx, &req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), type, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	return ubus_complete_request(ctx, &req, 0);

}

int ubus_subscribe(struct ubus_context *ctx, struct ubus_subscriber *obj, uint32_t id){
	return __ubus_subscribe_request(ctx, &obj->obj, id, UBUS_MSG_SUBSCRIBE);
}

int ubus_unsubscribe(struct ubus_context *ctx, struct ubus_subscriber *obj, uint32_t id){
	return __ubus_subscribe_request(ctx, &obj->obj, id, UBUS_MSG_UNSUBSCRIBE);
}
*/
