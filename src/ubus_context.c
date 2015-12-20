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

void ubus_init(struct ubus_context *self){
	assert(self); 
	memset(self, 0, sizeof(*self)); 	
	self->uloop = uloop_new(); 
	//uloop_add_fd(self->uloop, &self->sock, ULOOP_BLOCKING | ULOOP_READ);
	blob_buf_init(&self->buf, NULL, 0); 
}

void ubus_destroy(struct ubus_context *self){
	assert(self && self->uloop); 
	blob_buf_free(&self->buf);
	//close(self->sock.fd);
	//free(ctx->msgbuf.data);

	//uloop_remove_fd(self->uloop, &self->sock); 
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

int ubus_connect(struct ubus_context *ctx, const char *path){
	//if(ubus_socket_connect(ctx, path) < 0) {
//		return -1; 
//	}
	return 0; 
}

void ubus_handle_event(struct ubus_context *ctx){
	//ctx->sock.cb(&ctx->sock, ULOOP_READ);
}

static void ubus_queue_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf){
	struct ubus_pending_msg *pending;
	void *data;

	pending = calloc_a(sizeof(*pending), &data, blob_attr_raw_len(buf->data));

	pending->hdr.data = data;
	memcpy(&pending->hdr.hdr, &buf->hdr, sizeof(buf->hdr));
	memcpy(data, buf->data, blob_attr_raw_len(buf->data));
	//list_add(&pending->list, &ctx->pending);
	//if (ctx->sock.registered)
//		uloop_timeout_set(&ctx->pending_timer, 1);
}
/*
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
*/
static struct ubus_request *
ubus_find_request(struct ubus_context *ctx, uint32_t seq, uint32_t peer, int *id)
{
/*
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
	*/
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
	//bool registered = ctx->sock.registered;
	int status = UBUS_STATUS_NO_DATA;
	int64_t timeout = 0, time_end = 0;

	//if (!registered) {
	//	ubus_add_uloop(ctx);
	//}

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
		//ubus_poll_data(ctx, (unsigned int) timeout);

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
/*
	if (!registered) {
		uloop_remove_fd(ctx->uloop, &ctx->sock);

		if (!ctx->stack_depth)
			ctx->pending_timer.cb(&ctx->pending_timer);
	}
*/
	return status;
}
/*
void ubus_complete_deferred_request(struct ubus_context *ctx, struct ubus_request_data *req, int ret){
	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, ret); // STATUS
	blob_buf_put_i32(&ctx->buf, req->object); // objid
	ubus_send_msg(ctx, req->seq, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_STATUS, req->peer, req->fd);
}
*/
int ubus_send_reply(struct ubus_context *ctx, struct ubus_request_data *req,
		    struct blob_attr *msg)
{
	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, req->object); // objid
	// force table for replies
	//blob_attr_set_type(msg, BLOB_ATTR_TABLE); 
	blob_buf_put_attr(&ctx->buf, msg); 
	//ret = ubus_send_msg(ctx, req->seq, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_METHOD_RETURN, req->peer, -1);
	//if (ret < 0)
//		return UBUS_STATUS_NO_DATA;

	return 0;
}

int ubus_invoke_async(struct ubus_context *ctx, uint32_t obj, const char *method,
                       struct blob_attr *msg, struct ubus_request *req)
{
	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, obj);
	blob_buf_put_string(&ctx->buf, method);
	if (msg){
		// force table for outgoing arguments
		//blob_attr_set_type(msg, BLOB_ATTR_TABLE); 
		blob_buf_put_attr(&ctx->buf, msg);
	}
	printf("CALL "); 
	blob_attr_dump_json(blob_buf_head(&ctx->buf)); 
	
	printf(" test\n"); 
	fflush(stdout); 

	//if (ubus_start_request(ctx, req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_METHOD_CALL, obj) < 0)
//		return UBUS_STATUS_INVALID_ARGUMENT;

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
/*
static void
ubus_notify_complete_cb(struct ubus_request *req, int ret)
{
	struct ubus_notify_request *nreq;

	nreq = container_of(req, struct ubus_notify_request, req);
	if (!nreq->complete_cb)
		return;

	nreq->complete_cb(nreq, 0, 0);
}

static int _ubus_process_req_status(struct ubus_request *req, struct ubus_msghdr_buf *buf){
	int ret = UBUS_STATUS_INVALID_ARGUMENT;

	if(req->attrbuf[UBUS_ATTR_STATUS]){
		ret = blob_attr_get_i32(req->attrbuf[UBUS_ATTR_STATUS]); 
	}

	req->peer = buf->hdr.peer;
	ubus_request_set_status(req, ret);

	return ret;
}

static void _ubus_process_notify_status(struct ubus_request *req, int id, struct ubus_msghdr_buf *buf){
	struct ubus_notify_request *nreq;
	struct blob_attr *cur;
	int idx = 1;

	nreq = container_of(req, struct ubus_notify_request, req);
	nreq->pending &= ~(1 << id);

	if (!id) {
		// first id: ubusd's status message with a list of ids 
		if (req->attrbuf[UBUS_ATTR_SUBSCRIBERS]) {
			//blob_buf_for_each_attr(cur, tb[UBUS_ATTR_SUBSCRIBERS], rem) {
			for(cur = blob_attr_first_child(req->attrbuf[UBUS_ATTR_SUBSCRIBERS]); cur; cur = blob_attr_next_child(req->attrbuf[UBUS_ATTR_SUBSCRIBERS], cur)){
				if (!blob_attr_check_type(blob_attr_data(cur), blob_attr_len(cur), BLOB_ATTR_INT32))
					continue;

				nreq->pending |= (1 << idx);
				nreq->id[idx] = blob_attr_get_i32(cur);
				idx++;

				if (idx == UBUS_MAX_NOTIFY_PEERS + 1)
					break;
			}
		}
	} else {
		int status = blob_attr_get_i32(req->attrbuf[UBUS_ATTR_STATUS]); 
		if (nreq->status_cb)
			nreq->status_cb(nreq, id, status);
	}

	if (!nreq->pending)
		ubus_request_set_status(req, 0);
}

static void ubus_req_complete_cb(struct ubus_request *req){
	ubus_complete_handler_t cb = req->complete_cb;

	if (!cb)
		return;

	req->complete_cb = NULL;
	cb(req, req->status_code);
}
*/

static void _ubus_process_method_call(struct ubus_context *ctx, struct ubus_msghdr *hdr,
		    struct ubus_object *obj, struct blob_attr **attrbuf){
	struct ubus_request_data req = {
		.fd = -1,
	};
	int ret;

	if (!obj) {
		ret = UBUS_STATUS_NOT_FOUND;
		goto error;
	}

	if (!attrbuf[UBUS_ATTR_METHOD]) {
		ret = UBUS_STATUS_INVALID_ARGUMENT;
		goto error;
	}

	//if (attrbuf[UBUS_ATTR_NO_REPLY])
//		no_reply = blob_attr_get_i8(attrbuf[UBUS_ATTR_NO_REPLY]);

	req.peer = hdr->peer;
	req.seq = hdr->seq;
	req.object = obj->id;

	struct list_head *pos = NULL; 
	struct ubus_method *method = NULL; 

	list_for_each(pos, &obj->methods){
		method = container_of(pos, struct ubus_method, list); 
		if(strcmp(method->name, blob_attr_data(attrbuf[UBUS_ATTR_METHOD])) == 0){
			goto success; 
		}
	}

	/* not found */
	ret = UBUS_STATUS_METHOD_NOT_FOUND;
	goto error;
success:
	ret = method->handler(ctx, obj, &req,
	   blob_attr_data(attrbuf[UBUS_ATTR_METHOD]),
	   attrbuf[UBUS_ATTR_DATA]);
	// TODO: support deferred calls
	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, ret);
	blob_buf_put_i32(&ctx->buf, req.object);
	//ubus_send_msg(ctx, req.seq, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_METHOD_RETURN, req.peer, req.fd);
	return; 
error:
	// send error reply
	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, ret);
	blob_buf_put_i32(&ctx->buf, req.object);
	//ubus_send_msg(ctx, req.seq, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_ERROR, req.peer, req.fd);
}

static void _ubus_process_signal(struct ubus_context *ctx, struct ubus_msghdr *hdr,
		    struct ubus_object *obj, struct blob_attr **attrbuf){
	if (!obj || !attrbuf[UBUS_ATTR_ACTIVE])
		return;

	obj->has_subscribers = blob_attr_get_u8(attrbuf[UBUS_ATTR_ACTIVE]);
	if (obj->subscribe_cb)
		obj->subscribe_cb(ctx, obj);
}

static void _ubus_process_request(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, struct blob_attr **attrbuf)
{
	struct ubus_msghdr *hdr = &buf->hdr;
	struct ubus_object *obj;
	uint32_t objid;
	//void *prev_data = NULL;
	
	assert(ctx); 
	assert(buf); 
	assert(attrbuf); 

	if (!attrbuf[UBUS_ATTR_OBJID])
		return;

	objid = blob_attr_get_u32(attrbuf[UBUS_ATTR_OBJID]);
	obj = avl_find_element(&ctx->objects, &objid, obj, avl);
/*
	if (buf == &ctx->msgbuf) {
		prev_data = buf->data;
		buf->data = NULL;
	}
*/
	switch (hdr->type) {
		case UBUS_MSG_METHOD_CALL:
			_ubus_process_method_call(ctx, hdr, obj, attrbuf);
			break;
		case UBUS_MSG_SIGNAL:
			_ubus_process_signal(ctx, hdr, obj, attrbuf);
			break;
		default:
			return;
	}
/*
	if (prev_data) {
		if (buf->data)
			free(prev_data);
		else
			buf->data = prev_data;
	}
	*/
}

static void _ubus_process_reply(struct ubus_request *req, struct ubus_msghdr_buf *buf, struct blob_attr **attr){
	//if (!req->blocked) {
//		req->blocked = true;

		if (req->raw_data_cb)
			req->raw_data_cb(req, buf->hdr.type, buf->data);

		if (req->data_cb)
			req->data_cb(req, buf->hdr.type, req->attrbuf[UBUS_ATTR_DATA]);

	/*	struct ubus_pending_data *data;

		while (!list_empty(&req->pending)) {
			data = list_first_entry(&req->pending,
				struct ubus_pending_data, list);
			list_del(&data->list);
			if (!req->cancelled)
				req_data_cb(req, data->type, data->data);
			free(data);
		}
*/
		if(req->complete_cb)
			req->complete_cb(req, req->status_code);

		req->complete_cb = NULL;
//		req->blocked = false;

//		return;
//	}
/*
	len = blob_attr_raw_len(buf->data);
	data = calloc(1, sizeof(*data) + len);
	if (!data)
		return;

	data->type = buf->hdr.type;
	memcpy(data->data, buf->data, len);
	list_add(&data->list, &req->pending);
	*/
}

/*
void __hidden _ubus_process_reply(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, int fd, struct blob_attr **attrbuf){
	struct ubus_msghdr *hdr = &buf->hdr;
	struct ubus_request *req;
	int id = -1;

	req = ubus_find_request(ctx, hdr->seq, hdr->peer, &id);
	
	if (!req)
		return;

	memcpy(req->attrbuf, attrbuf, sizeof(attrbuf) * UBUS_ATTR_MAX); 

	switch(hdr->type) {
	case UBUS_MSG_STATUS:
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
*/
int ubus_start_request(struct ubus_context *ctx, struct ubus_request *req,
				void *msg, size_t size, int cmd, uint32_t peer)
{
	memset(req, 0, sizeof(*req));

	if (msg && blob_attr_pad_len(msg) > UBUS_MAX_MSGLEN)
		return -1;

	INIT_LIST_HEAD(&req->list);
	INIT_LIST_HEAD(&req->pending);
	req->ctx = ctx;
	req->peer = peer;
	req->seq = ++ctx->request_seq;
	return 0; 
	//return ubus_send_msg(ctx, req->seq, msg, size, cmd, peer, -1);
}
/*
void ubus_abort_request(struct ubus_context *ctx, struct ubus_request *req)
{
	if (list_empty(&req->list))
		return;

	req->cancelled = true;
	_ubus_process_req_reply(req);
	list_del_init(&req->list);
}
*/
void ubus_complete_request_async(struct ubus_context *ctx, struct ubus_request *req){
	if (!list_empty(&req->list))
		return;

	//list_add(&req->list, &ctx->requests);
}

void ubus_request_set_status(struct ubus_request *req, int ret){
	if (!list_empty(&req->list))
		list_del_init(&req->list);

	req->status_msg = true;
	req->status_code = ret;
	//if (!req->blocked)
//		ubus_req_complete_cb(req);
}

void ubus_message_parse(int type, struct blob_attr *msg, struct blob_attr **attrbuf){
	struct blob_attr *head = msg; 
	struct blob_attr *attr = NULL; 	
	switch(type) {
		case UBUS_MSG_HELLO: 
			attrbuf[UBUS_ATTR_OBJID] = attr = blob_attr_first_child(head); 
			break;
		case UBUS_MSG_METHOD_CALL:
			attrbuf[UBUS_ATTR_OBJID] = attr = blob_attr_first_child(head);  
			attrbuf[UBUS_ATTR_METHOD] = attr = blob_attr_next_child(head, attr);  
			attrbuf[UBUS_ATTR_DATA] = attr = blob_attr_next_child(head, attr); 
			break; 
		case UBUS_MSG_METHOD_RETURN:
			attrbuf[UBUS_ATTR_STATUS] = attr = blob_attr_first_child(head); 	
			attrbuf[UBUS_ATTR_OBJID] = attr = blob_attr_next_child(head, attr); 
			attrbuf[UBUS_ATTR_DATA] = attr = blob_attr_next_child(head, attr); 
			break; 
		case UBUS_MSG_ERROR: 
			attrbuf[UBUS_ATTR_STATUS] = attr = blob_attr_first_child(head); 
			break; 
		case UBUS_MSG_SIGNAL:
			break;
	}
}

void __hidden ubus_process_msg(struct ubus_context *ctx, struct ubus_msghdr_buf *buf, int fd){
	struct blob_attr *attrbuf[UBUS_ATTR_MAX];
	memset(attrbuf, 0, sizeof(attrbuf)); 

	printf("IN %s seq=%d peer=%08x: ", ubus_message_types[buf->hdr.type], buf->hdr.seq, buf->hdr.peer); 
	blob_attr_dump_json(buf->data); 

	ubus_message_parse(buf->hdr.type, buf->data, attrbuf); 

	switch(buf->hdr.type) {
		case UBUS_MSG_SIGNAL: 
		case UBUS_MSG_METHOD_CALL:
			if (ctx->stack_depth) {
				ubus_queue_msg(ctx, buf);
				break;
			}
			_ubus_process_request(ctx, buf, attrbuf);
			break;
		case UBUS_MSG_METHOD_RETURN: 
		case UBUS_MSG_ERROR: { 
			int id = -1; 
			struct ubus_request *req = ubus_find_request(ctx, buf->hdr.seq, buf->hdr.peer, &id);
			if (req && (req->data_cb || req->raw_data_cb))
				_ubus_process_reply(req, buf, attrbuf);
			break; 	
		}
	}
}

static void _ubus_lookup_id_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	uint32_t *id = req->priv;

	struct blob_attr *head = blob_attr_first_child(msg); 
	struct blob_attr *attr = blob_attr_next_child(head, blob_attr_first_child(head)); 

	*id = blob_attr_get_u32(attr);
}

int ubus_lookup_id(struct ubus_context *ctx, const char *path, uint32_t *id){
	blob_buf_reset(&ctx->buf);
	if (path)
		blob_buf_put_string(&ctx->buf, path);

	return ubus_invoke(ctx, 0, "ubus.directory.lookup", blob_buf_head(&ctx->buf), _ubus_lookup_id_cb, id, 0);
}

/*
struct ubus_lookup_request {
	struct ubus_request req;
	ubus_lookup_handler_t cb;
};

static void ubus_lookup_cb(struct ubus_request *ureq, int type, struct blob_attr *msg)
{
	struct ubus_lookup_request *req;
	struct ubus_object_data obj;

	req = container_of(ureq, struct ubus_lookup_request, req);

	msg = blob_attr_first_child(msg); 

	memset(&obj, 0, sizeof(obj));
	
	for(struct blob_attr *attr = blob_attr_first_child(msg); attr; attr = blob_attr_next_child(msg, attr)){
		const char *key = blob_attr_get_string(attr); attr = blob_attr_next_child(msg, attr); 
		if(strcmp(key, "id") == 0)	
			obj.id = blob_attr_get_u32(attr);
		else if(strcmp(key, "path") == 0)
			obj.path = blob_attr_get_string(attr); 
		else if(strcmp(key, "type") == 0)
			obj.type_id = blob_attr_get_u32(attr); 
		else if(strcmp(key, "methods") == 0)
			obj.signature = attr; 
		else if(strcmp(key, "client") == 0)
			obj.client_id = blob_attr_get_u32(attr); 
	}
	if(!obj.id || !obj.path || !obj.type_id){
		return; 
	}
	//obj.path = blob_attr_data(ureq->attrbuf[UBUS_ATTR_OBJPATH]);
	//obj.type_id = blob_attr_get_u32(ureq->attrbuf[UBUS_ATTR_OBJTYPE]);
	//obj.signature = ureq->attrbuf[UBUS_ATTR_SIGNATURE];
	req->cb(ureq->ctx, &obj, ureq->priv);
}

int ubus_lookup(struct ubus_context *ctx, const char *path,
		ubus_lookup_handler_t cb, void *priv)
{
	struct ubus_lookup_request lookup;

	blob_buf_reset(&ctx->buf);
	if (path)
		blob_buf_put_string(&ctx->buf, path);

	if (ubus_start_request(ctx, &lookup.req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_LOOKUP, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	lookup.req.raw_data_cb = ubus_lookup_cb;
	lookup.req.priv = priv;
	lookup.cb = cb;
	return ubus_complete_request(ctx, &lookup.req, 0);
}
*/
/*
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
		//obj->methods = calloc(1, sizeof(struct ubus_method)); 
		//ubus_method_init(obj->methods, NULL, ubus_event_cb); 

		//obj->n_methods = 1;

		//if (!!obj->name ^ !!obj->type)
	//		return UBUS_STATUS_INVALID_ARGUMENT;

		ret = ubus_add_object(ctx, obj);
		if (ret)
			return ret;
	}

	blob_buf_init(&b2, 0, 0);
	blob_buf_put_string(&b2, "object"); 
	blob_buf_put_u32(&b2, obj->id);
	if (pattern){
		blob_buf_put_string(&b2, "pattern"); 
		blob_buf_put_string(&b2, pattern);
	}

	ret = ubus_invoke(ctx, UBUS_SYSTEM_OBJECT_EVENT, "register", blob_buf_head(&b2), NULL, NULL, 0);
	blob_buf_free(&b2);

	return ret;
}

int ubus_send_event(struct ubus_context *ctx, const char *id,
		    struct blob_attr *data)
{
	struct ubus_request req;
	void *s;

	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, UBUS_SYSTEM_OBJECT_EVENT);
	blob_buf_put_string(&ctx->buf, "send");
	s = blob_buf_open_table(&ctx->buf);
	blob_buf_put_string(&ctx->buf, "id"); 
	blob_buf_put_string(&ctx->buf, id);
	blob_buf_put_string(&ctx->buf, "data"); 
	blob_buf_put_attr(&ctx->buf, data); 
	blob_buf_close_table(&ctx->buf, s);

	if (ubus_start_request(ctx, &req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_INVOKE, UBUS_SYSTEM_OBJECT_EVENT) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	return ubus_complete_request(ctx, &req, 0);
}
*/
