#include <stdint.h>
#include <unistd.h>
#include <zmq.h>
#include <uv.h>

#include "libubus2.h"

void ubus_init(struct ubus *self){
	memset(self, 0, sizeof(struct ubus)); 
	self->context = zmq_ctx_new(); 
}

void ubus_destroy(struct ubus *self){
	if(self->context) zmq_ctx_destroy(self->context); 
}

int ubus_connect(struct ubus *self, const char *path){
	if(self->socket) ubus_disconnect(self); 
	self->socket = zmq_socket(self->context, ZMQ_REQ); 
	if(!self->socket) return -EIO; 
	char url[255]; 
	snprintf(url, sizeof(url), "ipc:///etc/ubus/%s", path); 
	if(zmq_connect(self->socket, url) != 0) return -EIO;
	self->endpoint = strdup(url); 
	return 0; 
}

int ubus_disconnect(struct ubus *self){
	if(self->socket) {
		zmq_disconnect(self->socket, self->endpoint); 
		self->socket = 0; 
	}
	if(self->endpoint) {
		free(self->endpoint); 
		self->endpoint = 0; 
	}
	return 0; 
}

static void _do_ubus_call(uv_work_t *work){
	struct ubus_request *req = (struct ubus_request*) work->data; 
	struct ubus *self = req->ubus;
	char buf[1500]; 

	void *socket = zmq_socket(self->context, ZMQ_REQ); 
	if(!socket) goto error; 
	
	int tx = zmq_send(socket, req->args->head, blob_raw_len(req->args->head), 0); 
	if(tx != blob_raw_len(req->args->head)) goto error; 
	int rx = zmq_recv(socket, buf, sizeof(buf), 0); 
	if(rx < 0) goto error; 
	blob_buf_init(&req->resp, 0); 
	zmq_disconnect(socket, req->path); 
	return; 
error:;
}

void _after_ubus_call(uv_work_t *work, int status){
	struct ubus_request *req = (struct ubus_request*) work->data; 
	if(req->cb) req->cb(req, &req->resp); 
}

int ubus_call(struct ubus *self, const char *path, const char *method, struct blob_buf *args, ubus_response_handler_t cb){
	struct ubus_request *req = calloc(1, sizeof(struct ubus_request)); 
	ubus_request_init(req, path, method, args, cb); 
	req->method = strdup(method); 

	uv_work_t work = { .data = req }; 
	uv_queue_work(uv_default_loop(), &work, _do_ubus_call, _after_ubus_call); 
	
	return 0; 
}
