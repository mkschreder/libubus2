#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <blobpack/blobpack.h>

#include "ubus_method.h"
#include "ubus_context.h"
#include "ubus_message.h"
#include "ubus_request.h"
#include "ubus_socket.h"

struct ubus_pending_msg {
	struct list_head list;
	struct ubus_msghdr_buf hdr;
};

void ubus_init(struct ubus_context *self){
	memset(self, 0, sizeof(*self)); 	
	self->uloop = uloop_new(); 
	uloop_add_fd(self->uloop, &self->sock, ULOOP_BLOCKING | ULOOP_READ);
	blob_buf_init(&self->buf, NULL, 0); 
}

void ubus_destroy(struct ubus_context *self){
	assert(self && self->uloop); 
	blob_buf_free(&self->buf);
	close(self->sock.fd);
	//free(ctx->msgbuf.data);

	uloop_remove_fd(self->uloop, &self->sock); 
	uloop_delete(&self->uloop); 
}

struct ubus_context *ubus_new(void){
	struct ubus_context *self = malloc(sizeof(struct ubus_context)); 
	memset(self, 0, sizeof(struct ubus_context)); 
	ubus_init(self); 
	return self; 
}

void ubus_delete(struct ubus_context **self){
	ubus_destroy(*self); 
	free(*self); 
	*self = 0; 
}

void ubus_handle_event(struct ubus_context *ctx){
	ctx->sock.cb(&ctx->sock, ULOOP_READ);
}

int ubus_unregister_subscriber(struct ubus_context *ctx, struct ubus_subscriber *obj){
	return ubus_remove_object(ctx, &obj->obj);
}

static void ubus_queue_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf){
	struct ubus_pending_msg *pending;
	void *data;

	pending = calloc_a(sizeof(*pending), &data, blob_attr_raw_len(buf->data));

	pending->hdr.data = data;
	memcpy(&pending->hdr.hdr, &buf->hdr, sizeof(buf->hdr));
	memcpy(data, buf->data, blob_attr_raw_len(buf->data));
	list_add(&pending->list, &ctx->pending);
	if (ctx->sock.registered)
		uloop_timeout_set(&ctx->pending_timer, 1);
}

static int
ubus_find_notify_id(struct ubus_notify_request *n, uint32_t objid)
{
	uint32_t pending = n->pending;
	int i;

	for (i = 0; pending; i++, pending >>= 1) {
		if (!(pending & 1))
			continue;

		if (n->id[i] == objid)
			return i;
	}

	return -1;
}

static struct ubus_request *
ubus_find_request(struct ubus_context *ctx, uint32_t seq, uint32_t peer, int *id)
{
	struct ubus_request *req;

	list_for_each_entry(req, &ctx->requests, list) {
		struct ubus_notify_request *nreq;
		nreq = container_of(req, struct ubus_notify_request, req);

		if (seq != req->seq)
			continue;

		if (req->notify) {
			if (!nreq->pending)
				continue;

			*id = ubus_find_notify_id(nreq, peer);
			if (*id < 0)
				continue;
		} else if (peer != req->peer)
			continue;

		return req;
	}
	return NULL;
}

static int64_t get_time_msec(void)
{
	struct timespec ts;
	int64_t val;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	val = (int64_t) ts.tv_sec * 1000LL;
	val += ts.tv_nsec / 1000000LL;
	return val;
}

static void ubus_sync_req_cb(struct ubus_request *req, int ret)
{
	req->status_msg = true;
	req->status_code = ret;
}

int ubus_complete_request(struct ubus_context *ctx, struct ubus_request *req,
			  int req_timeout)
{
	ubus_complete_handler_t complete_cb = req->complete_cb;
	bool registered = ctx->sock.registered;
	int status = UBUS_STATUS_NO_DATA;
	int64_t timeout = 0, time_end = 0;

	/*
	if (!registered) {
		uloop_init();
		ubus_add_uloop(ctx);
	}*/

	if (req_timeout)
		time_end = get_time_msec() + req_timeout;

	ubus_complete_request_async(ctx, req);
	req->complete_cb = ubus_sync_req_cb;

	ctx->stack_depth++;
	while (!req->status_msg) {
		//bool cancelled = uloop_cancelled;

		//uloop_cancelled = false;
		if (req_timeout) {
			timeout = time_end - get_time_msec();
			if (timeout <= 0) {
				ubus_request_set_status(req, UBUS_STATUS_TIMEOUT);
				//uloop_cancelled = cancelled;
				break;
			}
		}
		ubus_poll_data(ctx, (unsigned int) timeout);

		//uloop_cancelled = cancelled;
	}
	ctx->stack_depth--;
	//if (ctx->stack_depth)
//		uloop_cancelled = true;

	if (req->status_msg)
		status = req->status_code;

	req->complete_cb = complete_cb;
	if (req->complete_cb)
		req->complete_cb(req, status);

	if (!registered) {
		uloop_remove_fd(ctx->uloop, &ctx->sock);

		if (!ctx->stack_depth)
			ctx->pending_timer.cb(&ctx->pending_timer);
	}

	return status;
}

void ubus_complete_deferred_request(struct ubus_context *ctx, struct ubus_request_data *req, int ret)
{
	blob_buf_reset(&ctx->buf, 0);
	blob_buf_put_int32(&ctx->buf, UBUS_ATTR_STATUS, ret);
	blob_buf_put_int32(&ctx->buf, UBUS_ATTR_OBJID, req->object);
	ubus_send_msg(ctx, req->seq, ctx->buf.head, UBUS_MSG_STATUS, req->peer, req->fd);
}

int ubus_send_reply(struct ubus_context *ctx, struct ubus_request_data *req,
		    struct blob_attr *msg)
{
	int ret;

	blob_buf_reset(&ctx->buf, 0);
	blob_buf_put_int32(&ctx->buf, UBUS_ATTR_OBJID, req->object);
	blob_buf_put(&ctx->buf, UBUS_ATTR_DATA, blob_attr_data(msg), blob_attr_len(msg));
	ret = ubus_send_msg(ctx, req->seq, ctx->buf.head, UBUS_MSG_DATA, req->peer, -1);
	if (ret < 0)
		return UBUS_STATUS_NO_DATA;

	return 0;
}

int ubus_invoke_async(struct ubus_context *ctx, uint32_t obj, const char *method,
                       struct blob_attr *msg, struct ubus_request *req)
{
	blob_buf_reset(&ctx->buf, 0);
	blob_buf_put_int32(&ctx->buf, UBUS_ATTR_OBJID, obj);
	blob_buf_put_string(&ctx->buf, UBUS_ATTR_METHOD, method);
	if (msg)
		blob_buf_put(&ctx->buf, UBUS_ATTR_DATA, blob_attr_data(msg), blob_attr_len(msg));

	if (ubus_start_request(ctx, req, ctx->buf.head, UBUS_MSG_INVOKE, obj) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	return 0;
}

int ubus_invoke(struct ubus_context *ctx, uint32_t obj, const char *method,
                struct blob_attr *msg, ubus_data_handler_t cb, void *priv,
		int timeout)
{
	struct ubus_request req;
	int rc;

	rc = ubus_invoke_async(ctx, obj, method, msg, &req);
	if (rc)
		return rc;

	req.data_cb = cb;
	req.priv = priv;
	return ubus_complete_request(ctx, &req, timeout);
}

static void
ubus_notify_complete_cb(struct ubus_request *req, int ret)
{
	struct ubus_notify_request *nreq;

	nreq = container_of(req, struct ubus_notify_request, req);
	if (!nreq->complete_cb)
		return;

	nreq->complete_cb(nreq, 0, 0);
}


static int
__ubus_notify_async(struct ubus_context *ctx, struct ubus_object *obj,
		    const char *type, struct blob_attr *msg,
		    struct ubus_notify_request *req, bool reply)
{
	memset(req, 0, sizeof(*req));

	blob_buf_reset(&ctx->buf, 0);
	blob_buf_put_int32(&ctx->buf, UBUS_ATTR_OBJID, obj->id);
	blob_buf_put_string(&ctx->buf, UBUS_ATTR_METHOD, type);

	if (!reply)
		blob_buf_put_int8(&ctx->buf, UBUS_ATTR_NO_REPLY, true);

	if (msg)
		blob_buf_put(&ctx->buf, UBUS_ATTR_DATA, blob_attr_data(msg), blob_attr_len(msg));

	if (ubus_start_request(ctx, &req->req, ctx->buf.head, UBUS_MSG_NOTIFY, obj->id) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	/* wait for status message from ubusd first */
	req->req.notify = true;
	req->pending = 1;
	req->id[0] = obj->id;
	req->req.complete_cb = ubus_notify_complete_cb;

	return 0;
}
int ubus_notify_async(struct ubus_context *ctx, struct ubus_object *obj,
		      const char *type, struct blob_attr *msg,
		      struct ubus_notify_request *req)
{
	return __ubus_notify_async(ctx, obj, type, msg, req, true);
}

int ubus_notify(struct ubus_context *ctx, struct ubus_object *obj,
		const char *type, struct blob_attr *msg, int timeout)
{
	struct ubus_notify_request req;
	int ret;

	ret = __ubus_notify_async(ctx, obj, type, msg, &req, timeout >= 0);
	if (ret < 0)
		return ret;

	if (timeout < 0) {
		ubus_abort_request(ctx, &req.req);
		return 0;
	}

	return ubus_complete_request(ctx, &req.req, timeout);
}



static bool _ubus_header_get_status(struct ubus_msghdr_buf *buf, int *ret){
	struct blob_attr **attrbuf = ubus_parse_msg(buf->data);

	if (!attrbuf[UBUS_ATTR_STATUS])
		return false;

	*ret = blob_attr_get_u32(attrbuf[UBUS_ATTR_STATUS]);
	return true;
}

static int _ubus_process_req_status(struct ubus_request *req, struct ubus_msghdr_buf *buf){
	int ret = UBUS_STATUS_INVALID_ARGUMENT;

	_ubus_header_get_status(buf, &ret);
	req->peer = buf->hdr.peer;
	ubus_request_set_status(req, ret);

	return ret;
}

static void _ubus_process_notify_status(struct ubus_request *req, int id, struct ubus_msghdr_buf *buf){
	struct ubus_notify_request *nreq;
	struct blob_attr **tb;
	struct blob_attr *cur;
	int rem, idx = 1;
	int ret = 0;

	nreq = container_of(req, struct ubus_notify_request, req);
	nreq->pending &= ~(1 << id);

	if (!id) {
		/* first id: ubusd's status message with a list of ids */
		tb = ubus_parse_msg(buf->data);
		if (tb[UBUS_ATTR_SUBSCRIBERS]) {
			blob_buf_for_each_attr(cur, tb[UBUS_ATTR_SUBSCRIBERS], rem) {
				if (!blob_attr_check_type(blob_attr_data(cur), blob_attr_len(cur), BLOB_ATTR_INT32))
					continue;

				nreq->pending |= (1 << idx);
				nreq->id[idx] = blob_attr_get_int32(cur);
				idx++;

				if (idx == UBUS_MAX_NOTIFY_PEERS + 1)
					break;
			}
		}
	} else {
		_ubus_header_get_status(buf, &ret);
		if (nreq->status_cb)
			nreq->status_cb(nreq, id, ret);
	}

	if (!nreq->pending)
		ubus_request_set_status(req, 0);
}

static void req_data_cb(struct ubus_request *req, int type, struct blob_attr *data)
{
	struct blob_attr **attr;

	if (req->raw_data_cb)
		req->raw_data_cb(req, type, data);

	if (!req->data_cb)
		return;

	attr = ubus_parse_msg(data);
	req->data_cb(req, type, attr[UBUS_ATTR_DATA]);
}

static void __ubus_process_req_data(struct ubus_request *req){
	struct ubus_pending_data *data;

	while (!list_empty(&req->pending)) {
		data = list_first_entry(&req->pending,
			struct ubus_pending_data, list);
		list_del(&data->list);
		if (!req->cancelled)
			req_data_cb(req, data->type, data->data);
		free(data);
	}
}


static void ubus_req_complete_cb(struct ubus_request *req){
	ubus_complete_handler_t cb = req->complete_cb;

	if (!cb)
		return;

	req->complete_cb = NULL;
	cb(req, req->status_code);
}


static void _ubus_process_req_data(struct ubus_request *req, struct ubus_msghdr_buf *buf){
	struct ubus_pending_data *data;
	int len;

	if (!req->blocked) {
		req->blocked = true;
		req_data_cb(req, buf->hdr.type, buf->data);
		__ubus_process_req_data(req);
		req->blocked = false;

		if (req->status_msg)
			ubus_req_complete_cb(req);

		return;
	}

	len = blob_attr_raw_len(buf->data);
	data = calloc(1, sizeof(*data) + len);
	if (!data)
		return;

	data->type = buf->hdr.type;
	memcpy(data->data, buf->data, len);
	list_add(&data->list, &req->pending);
}


void __hidden _ubus_process_req_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, int fd){
	struct ubus_msghdr *hdr = &buf->hdr;
	struct ubus_request *req;
	int id = -1;

	switch(hdr->type) {
	case UBUS_MSG_STATUS:
		req = ubus_find_request(ctx, hdr->seq, hdr->peer, &id);
		if (!req)
			break;

		if (fd >= 0) {
			if (req->fd_cb)
				req->fd_cb(req, fd);
			else
				close(fd);
		}

		if (id >= 0)
			_ubus_process_notify_status(req, id, buf);
		else
			_ubus_process_req_status(req, buf);
		break;

	case UBUS_MSG_DATA:
		req = ubus_find_request(ctx, hdr->seq, hdr->peer, &id);
		if (req && (req->data_cb || req->raw_data_cb))
			_ubus_process_req_data(req, buf);
		break;
	}
}

int ubus_start_request(struct ubus_context *ctx, struct ubus_request *req,
				struct blob_attr *msg, int cmd, uint32_t peer)
{
	memset(req, 0, sizeof(*req));

	if (msg && blob_attr_pad_len(msg) > UBUS_MAX_MSGLEN)
		return -1;

	INIT_LIST_HEAD(&req->list);
	INIT_LIST_HEAD(&req->pending);
	req->ctx = ctx;
	req->peer = peer;
	req->seq = ++ctx->request_seq;
	return ubus_send_msg(ctx, req->seq, msg, cmd, peer, -1);
}

void ubus_abort_request(struct ubus_context *ctx, struct ubus_request *req)
{
	if (list_empty(&req->list))
		return;

	req->cancelled = true;
	__ubus_process_req_data(req);
	list_del_init(&req->list);
}

void ubus_complete_request_async(struct ubus_context *ctx, struct ubus_request *req)
{
	if (!list_empty(&req->list))
		return;

	list_add(&req->list, &ctx->requests);
}
void ubus_request_set_status(struct ubus_request *req, int ret){
	if (!list_empty(&req->list))
		list_del_init(&req->list);

	req->status_msg = true;
	req->status_code = ret;
	if (!req->blocked)
		ubus_req_complete_cb(req);
}


void __hidden ubus_process_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, int fd){
	switch(buf->hdr.type) {
	case UBUS_MSG_STATUS:
	case UBUS_MSG_DATA:
		_ubus_process_req_msg(ctx, buf, fd);
		break;

	case UBUS_MSG_INVOKE:
	case UBUS_MSG_UNSUBSCRIBE:
	case UBUS_MSG_NOTIFY:
		if (ctx->stack_depth) {
			ubus_queue_msg(ctx, buf);
			break;
		}

		ubus_process_obj_msg(ctx, buf);
		break;
	}
}

static void _ubus_process_pending_msg(struct uloop_timeout *timeout){
	struct ubus_context *ctx = container_of(timeout, struct ubus_context, pending_timer);
	struct ubus_pending_msg *pending;

	while (!ctx->stack_depth && !list_empty(&ctx->pending)) {
		pending = list_first_entry(&ctx->pending, struct ubus_pending_msg, list);
		list_del(&pending->list);
		ubus_process_msg(ctx, &pending->hdr, -1);
		free(pending);
	}
}

struct ubus_lookup_request {
	struct ubus_request req;
	ubus_lookup_handler_t cb;
};

static void ubus_lookup_cb(struct ubus_request *ureq, int type, struct blob_attr *msg)
{
	struct ubus_lookup_request *req;
	struct ubus_object_data obj;
	struct blob_attr **attr;

	req = container_of(ureq, struct ubus_lookup_request, req);
	attr = ubus_parse_msg(msg);

	if (!attr[UBUS_ATTR_OBJID] || !attr[UBUS_ATTR_OBJPATH] ||
	    !attr[UBUS_ATTR_OBJTYPE])
		return;

	memset(&obj, 0, sizeof(obj));
	obj.id = blob_attr_get_u32(attr[UBUS_ATTR_OBJID]);
	obj.path = blob_attr_data(attr[UBUS_ATTR_OBJPATH]);
	obj.type_id = blob_attr_get_u32(attr[UBUS_ATTR_OBJTYPE]);
	obj.signature = attr[UBUS_ATTR_SIGNATURE];
	req->cb(ureq->ctx, &obj, ureq->priv);
}

int ubus_lookup(struct ubus_context *ctx, const char *path,
		ubus_lookup_handler_t cb, void *priv)
{
	struct ubus_lookup_request lookup;

	blob_buf_reset(&ctx->buf, 0);
	if (path)
		blob_buf_put_string(&ctx->buf, UBUS_ATTR_OBJPATH, path);

	if (ubus_start_request(ctx, &lookup.req, ctx->buf.head, UBUS_MSG_LOOKUP, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	lookup.req.raw_data_cb = ubus_lookup_cb;
	lookup.req.priv = priv;
	lookup.cb = cb;
	return ubus_complete_request(ctx, &lookup.req, 0);
}

static void ubus_lookup_id_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct blob_attr **attr;
	uint32_t *id = req->priv;

	attr = ubus_parse_msg(msg);

	if (!attr[UBUS_ATTR_OBJID])
		return;

	*id = blob_attr_get_u32(attr[UBUS_ATTR_OBJID]);
}

int ubus_lookup_id(struct ubus_context *ctx, const char *path, uint32_t *id)
{
	struct ubus_request req;

	blob_buf_reset(&ctx->buf, 0);
	if (path)
		blob_buf_put_string(&ctx->buf, UBUS_ATTR_OBJPATH, path);

	if (ubus_start_request(ctx, &req, ctx->buf.head, UBUS_MSG_LOOKUP, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	req.raw_data_cb = ubus_lookup_id_cb;
	req.priv = id;

	return ubus_complete_request(ctx, &req, 0);
}

static int ubus_event_cb(struct ubus_context *ctx, struct ubus_object *obj,
			 struct ubus_request_data *req,
			 const char *method, struct blob_attr *msg)
{
	struct ubus_event_handler *ev;

	ev = container_of(obj, struct ubus_event_handler, obj);
	ev->cb(ctx, ev, method, msg);
	return 0;
}

int ubus_register_event_handler(struct ubus_context *ctx,
				struct ubus_event_handler *ev,
				const char *pattern){
	assert(ctx && ev); 

	struct ubus_object *obj = &ev->obj;
	struct blob_buf b2;
	int ret;

	if (!obj->id) {
		obj->methods = calloc(1, sizeof(struct ubus_method)); 
		ubus_method_init(obj->methods, NULL, ubus_event_cb); 

		obj->n_methods = 1;

		if (!!obj->name ^ !!obj->type)
			return UBUS_STATUS_INVALID_ARGUMENT;

		ret = ubus_add_object(ctx, obj);
		if (ret)
			return ret;
	}

	/* use a second buffer, ubus_invoke() overwrites the primary one */
	blob_buf_init(&b2, 0, 0);
	blobmsg_add_u32(&b2, "object", obj->id);
	if (pattern)
		blobmsg_add_string(&b2, "pattern", pattern);

	ret = ubus_invoke(ctx, UBUS_SYSTEM_OBJECT_EVENT, "register", b2.head,
			  NULL, NULL, 0);
	blob_buf_free(&b2);

	return ret;
}

int ubus_send_event(struct ubus_context *ctx, const char *id,
		    struct blob_attr *data)
{
	struct ubus_request req;
	void *s;

	blob_buf_reset(&ctx->buf, 0);
	blob_buf_put_int32(&ctx->buf, UBUS_ATTR_OBJID, UBUS_SYSTEM_OBJECT_EVENT);
	blob_buf_put_string(&ctx->buf, UBUS_ATTR_METHOD, "send");
	s = blob_buf_nest_start(&ctx->buf, UBUS_ATTR_DATA);
	blobmsg_add_string(&ctx->buf, "id", id);
	blobmsg_add_field(&ctx->buf, BLOBMSG_TYPE_TABLE, "data", blob_attr_data(data), blob_attr_len(data));
	blob_buf_nest_end(&ctx->buf, s);

	if (ubus_start_request(ctx, &req, ctx->buf.head, UBUS_MSG_INVOKE, UBUS_SYSTEM_OBJECT_EVENT) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	return ubus_complete_request(ctx, &req, 0);
}

static void _ubus_default_connection_lost(struct ubus_context *ctx)
{
	if (ctx->sock.registered)
		uloop_delete(&ctx->uloop);
}

//struct blob_buf b __hidden = {};
static int ubus_cmp_id(const void *k1, const void *k2, void *ptr){
	const uint32_t *id1 = k1, *id2 = k2;

	if (*id1 < *id2)
		return -1;
	else
		return *id1 > *id2;
}

int ubus_connect(struct ubus_context *ctx, const char *path)
{
	ctx->sock.fd = -1;
	ctx->sock.cb = ubus_handle_data;
	ctx->connection_lost = _ubus_default_connection_lost;
	ctx->pending_timer.cb = _ubus_process_pending_msg;

	ctx->msgbuf.data = calloc(UBUS_MSG_CHUNK_SIZE, sizeof(char));
	if (!ctx->msgbuf.data)
		return -1;
	ctx->msgbuf_data_len = UBUS_MSG_CHUNK_SIZE;

	INIT_LIST_HEAD(&ctx->requests);
	INIT_LIST_HEAD(&ctx->pending);
	avl_init(&ctx->objects, ubus_cmp_id, false, NULL);
	if (ubus_reconnect(ctx, path)) {
		free(ctx->msgbuf.data);
		return -1;
	}

	return 0;
}

static void ubus_auto_reconnect_cb(struct uloop_timeout *timeout)
{
	//struct ubus_auto_conn *conn = container_of(timeout, struct ubus_auto_conn, timer);

	fprintf(stderr, "%s: fix autoreconnect!\n", __FUNCTION__); 

	//if (!ubus_reconnect(&conn->ctx, conn->path))
		//ubus_add_uloop(&conn->ctx);
	//else
	//		uloop_timeout_set(timeout, 1000);
}

static void ubus_auto_disconnect_cb(struct ubus_context *ctx)
{
	struct ubus_auto_conn *conn = container_of(ctx, struct ubus_auto_conn, ctx);

	conn->timer.cb = ubus_auto_reconnect_cb;
	uloop_timeout_set(&conn->timer, 1000);
}

static void ubus_auto_connect_cb(struct uloop_timeout *timeout)
{
	struct ubus_auto_conn *conn = container_of(timeout, struct ubus_auto_conn, timer);

	if (ubus_connect(&conn->ctx, conn->path)) {
		uloop_timeout_set(timeout, 1000);
		fprintf(stderr, "failed to connect to ubus\n");
		return;
	}
	conn->ctx.connection_lost = ubus_auto_disconnect_cb;
	if (conn->cb)
		conn->cb(&conn->ctx);
	//ubus_add_uloop(&conn->ctx);
}

void ubus_auto_connect(struct ubus_auto_conn *conn)
{
	conn->timer.cb = ubus_auto_connect_cb;
	ubus_auto_connect_cb(&conn->timer);
}

