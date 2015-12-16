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

#include "libubus2.h"

#include "ubus_context.h"

static void
ubus_process_unsubscribe(struct ubus_context *ctx, struct ubus_msghdr *hdr,
			 struct ubus_object *obj, struct blob_attr **attrbuf)
{
	struct ubus_subscriber *s;

	if (!obj || !attrbuf[UBUS_ATTR_TARGET])
		return;

	s = container_of(obj, struct ubus_subscriber, obj);
	
	if (obj->methods != s->watch_method)
		return;

	if (s->remove_cb)
		s->remove_cb(ctx, s, blob_attr_get_u32(attrbuf[UBUS_ATTR_TARGET]));
}


static void
ubus_process_notify(struct ubus_context *ctx, struct ubus_msghdr *hdr,
		    struct ubus_object *obj, struct blob_attr **attrbuf)
{
	if (!obj || !attrbuf[UBUS_ATTR_ACTIVE])
		return;

	obj->has_subscribers = blob_attr_get_u8(attrbuf[UBUS_ATTR_ACTIVE]);
	if (obj->subscribe_cb)
		obj->subscribe_cb(ctx, obj);
}
static void
ubus_process_invoke(struct ubus_context *ctx, struct ubus_msghdr *hdr,
		    struct ubus_object *obj, struct blob_attr **attrbuf)
{
	struct ubus_request_data req = {
		.fd = -1,
	};
	int method;
	int ret;
	bool no_reply = false;

	if (!obj) {
		ret = UBUS_STATUS_NOT_FOUND;
		goto send;
	}

	if (!attrbuf[UBUS_ATTR_METHOD]) {
		ret = UBUS_STATUS_INVALID_ARGUMENT;
		goto send;
	}

	if (attrbuf[UBUS_ATTR_NO_REPLY])
		no_reply = blob_attr_get_i8(attrbuf[UBUS_ATTR_NO_REPLY]);

	req.peer = hdr->peer;
	req.seq = hdr->seq;
	req.object = obj->id;

	for (method = 0; method < obj->n_methods; method++)
		if (!obj->methods[method].name ||
		    !strcmp(obj->methods[method].name,
		            blob_attr_data(attrbuf[UBUS_ATTR_METHOD])))
			goto found;

	/* not found */
	ret = UBUS_STATUS_METHOD_NOT_FOUND;
	goto send;

found:
	ret = obj->methods[method].handler(ctx, obj, &req,
					   blob_attr_data(attrbuf[UBUS_ATTR_METHOD]),
					   attrbuf[UBUS_ATTR_DATA]);
	if (req.deferred || no_reply)
		return;

send:
	ubus_complete_deferred_request(ctx, &req, ret);
}

void __hidden ubus_process_obj_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, struct blob_attr **attrbuf)
{
	void (*cb)(struct ubus_context *, struct ubus_msghdr *,
		   struct ubus_object *, struct blob_attr **);
	struct ubus_msghdr *hdr = &buf->hdr;
	struct ubus_object *obj;
	uint32_t objid;
	void *prev_data = NULL;

	if (!attrbuf[UBUS_ATTR_OBJID])
		return;

	objid = blob_attr_get_u32(attrbuf[UBUS_ATTR_OBJID]);
	obj = avl_find_element(&ctx->objects, &objid, obj, avl);

	switch (hdr->type) {
	case UBUS_MSG_INVOKE:
		cb = ubus_process_invoke;
		break;
	case UBUS_MSG_UNSUBSCRIBE:
		cb = ubus_process_unsubscribe;
		break;
	case UBUS_MSG_NOTIFY:
		cb = ubus_process_notify;
		break;
	default:
		return;
	}

	if (buf == &ctx->msgbuf) {
		prev_data = buf->data;
		buf->data = NULL;
	}

	cb(ctx, hdr, obj, attrbuf);

	if (prev_data) {
		if (buf->data)
			free(prev_data);
		else
			buf->data = prev_data;
	}
}

static void ubus_add_object_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct ubus_object *obj = req->priv;

	if (!req->attrbuf[UBUS_ATTR_OBJID]) {
		printf("no object id returned!\n"); 
		return;
	}

	obj->id = blob_attr_get_u32(req->attrbuf[UBUS_ATTR_OBJID]);
	
	printf("added object id %d\n", obj->id); 

	if (req->attrbuf[UBUS_ATTR_OBJTYPE]) {
		obj->type->id = blob_attr_get_u32(req->attrbuf[UBUS_ATTR_OBJTYPE]);
	}  else {
		printf("no type returned!\n"); 
	}
	obj->avl.key = &obj->id;
	avl_insert(&req->ctx->objects, &obj->avl);
}

static void ubus_push_method_data(struct ubus_context *ctx, const struct ubus_method *m)
{
	void *mtbl;
	int i;

	blob_buf_put_string(&ctx->buf, m->name); 
	mtbl = blob_buf_open_table(&ctx->buf);

	for (i = 0; i < m->n_policy; i++) {
		if (m->mask && !(m->mask & (1 << i)))
			continue;
		blob_buf_put_string(&ctx->buf, m->policy[i].name); 
		blob_buf_put_u32(&ctx->buf, m->policy[i].type);
	}

	blob_buf_close_table(&ctx->buf, mtbl);
}

static bool ubus_push_object_type(struct ubus_context *ctx, const struct ubus_object_type *type)
{
	void *s;
	int i;

	s = blob_buf_open_table(&ctx->buf);

	for (i = 0; i < type->n_methods; i++)
		ubus_push_method_data(ctx, &type->methods[i]);

	blob_buf_close_table(&ctx->buf, s);

	return true;
}

int ubus_add_object(struct ubus_context *ctx, struct ubus_object *obj){
	assert(ctx && obj); 

	struct ubus_request req;
	int ret;

	blob_buf_reset(&ctx->buf);

	if (obj->name && obj->type) {
		blob_buf_put_string(&ctx->buf, obj->name);

		if (obj->type->id){
			printf("pushing known id %x\n", obj->type->id); 
			blob_buf_put_i32(&ctx->buf, obj->type->id);
		} 
		else {
			printf("pushing signature\n"); 
			if (!ubus_push_object_type(ctx, obj->type))
				return UBUS_STATUS_INVALID_ARGUMENT;
		}
	}
	
	if (ubus_start_request(ctx, &req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_ADD_OBJECT, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	req.raw_data_cb = ubus_add_object_cb;
	req.priv = obj;
	ret = ubus_complete_request(ctx, &req, 0);
	if (ret)
		return ret;

	if (!obj->id)
		return UBUS_STATUS_NO_DATA;

	return 0;
}

static void ubus_remove_object_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct ubus_object *obj = req->priv;

	if (!req->attrbuf[UBUS_ATTR_OBJID])
		return;

	obj->id = 0;

	if (req->attrbuf[UBUS_ATTR_OBJTYPE] && obj->type)
		obj->type->id = 0;

	avl_delete(&req->ctx->objects, &obj->avl);
}

int ubus_remove_object(struct ubus_context *ctx, struct ubus_object *obj)
{
	struct ubus_request req;
	int ret;

	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, obj->id);

	if (ubus_start_request(ctx, &req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_REMOVE_OBJECT, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	req.raw_data_cb = ubus_remove_object_cb;
	req.priv = obj;
	ret = ubus_complete_request(ctx, &req, 0);
	if (ret)
		return ret;

	if (obj->id)
		return UBUS_STATUS_NO_DATA;

	return 0;
}
