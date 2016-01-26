/*
 * Copyright (C) 2015 Martin Schröder <mkschreder.uk@gmail.com>
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

#include <libutype/avl-cmp.h>
#include "ubus_context.h"
#include "ubus_srv.h"
#include "ubus_peer.h"

struct ubus_peer *_find_peer_by_name(struct ubus_context *self, const char *client_name){
	struct avl_node *avl = avl_find(&self->peers_by_name, client_name); 
	if(!avl) return NULL; 
	return container_of(avl, struct ubus_peer, avl_name); 
}

struct ubus_peer *_find_peer_by_id(struct ubus_context *self, uint32_t id){
	struct avl_node *avl = avl_find(&self->peers_by_id, &id); 
	if(!avl) return NULL; 
	return container_of(avl, struct ubus_peer, avl_id); 
}

struct ubus_peer *_create_peer(struct ubus_context *self, uint32_t id){
	char client_name[256]; 
	snprintf(client_name, sizeof(client_name), "00%08x", id); 
	struct ubus_peer *peer = ubus_peer_new(client_name, id); 
	//printf("inserting peer %s %08x\n", client_name, id); 
	if(avl_insert(&self->peers_by_id, &peer->avl_id) != 0){
		//printf("could not insert peer by id!\n"); 
		ubus_peer_delete(&peer); 
		return NULL; 
	}
	if(avl_insert(&self->peers_by_name, &peer->avl_name) != 0){
		//printf("could not insert peer by name!\n"); 
		avl_delete(&self->peers_by_id, &peer->avl_id); 
		ubus_peer_delete(&peer); 
		return NULL; 
	}

	return peer; 
}
/*
struct ubus_object *_find_object_by_name(struct ubus_context *self, const char *path){
	struct avl_node *avl = avl_find(&self->objects_by_name, path); 
	if(!avl) return NULL; 
	return container_of(avl, struct ubus_object, avl); 
}
*/
static void _send_well_known_name(struct ubus_context *self, uint32_t peer);

void _on_msg_signal(struct ubus_context *self, struct ubus_peer *peer, const char *method, struct blob_field *msg){
	// first argument is always signal type
	if(!method) return; 

	if(strcmp(method, "ubus.peer.well_known_name") == 0){
		struct blob_field *attr = blob_field_first_child(msg); 
		const char *peer_name = blob_field_get_string(attr); 	
		// do not allow changing name!
		struct avl_node *avl = avl_find(&self->peers_by_name, peer_name); 
		if(avl) return; 
		
		avl_delete(&self->peers_by_name, &peer->avl_name); 
		ubus_peer_set_name(peer, peer_name); 
		if(0 != avl_insert(&self->peers_by_name, &peer->avl_name)){
			// TODO: handle duplicates
		}
		// TODO: handle errors and make sure peers are authenticated
		printf("renamed peer %s\n", peer_name); 

		// send our name to the other peer as well
		_send_well_known_name(self, peer->id); 
	} 
}

static void _on_resolve_method_call(struct ubus_request *req, struct blob_field *data){
	struct ubus_context *self = (struct ubus_context*)ubus_request_get_userdata(req); 
	
	struct ubus_message *msg = ubus_socket_new_message(self->socket); 
	msg->peer = req->src_id; 
	blob_reset(&msg->buf); 
	blob_set_type(&msg->buf, BLOB_FIELD_TABLE); 
	blob_put_string(&msg->buf, "jsonrpc"); 
	blob_put_string(&msg->buf, "2.0"); 
	blob_put_string(&msg->buf, "id"); 
	blob_put_int(&msg->buf, req->seq); 
	blob_put_string(&msg->buf, "result"); 
	blob_put_attr(&msg->buf, data); 

	if(ubus_socket_send(self->socket, &msg) < 0){
		printf("resolve failed\n"); 
	}
}

static void _ubus_send_error(struct ubus_context *self, uint32_t peer, uint32_t seq, struct blob_field *data){
	struct ubus_message *msg = ubus_socket_new_message(self->socket); 
	msg->peer = peer; 
	blob_reset(&msg->buf); 
	blob_set_type(&msg->buf, BLOB_FIELD_TABLE); 
	blob_put_string(&msg->buf, "jsonrpc"); 
	blob_put_string(&msg->buf, "2.0"); 
	blob_put_string(&msg->buf, "id"); 
	blob_put_int(&msg->buf, seq); 
	blob_put_string(&msg->buf, "error"); 
	blob_put_attr(&msg->buf, data); 

	if(ubus_socket_send(self->socket, &msg) < 0){
		printf("send error failed\n"); 
	}
}

static int _ubus_send_request(struct ubus_context *self, uint32_t peer, uint32_t seq, const char *rpc_method, const char *object, const char *method, struct blob_field *data){
	struct ubus_message *msg = ubus_socket_new_message(self->socket); 
	msg->peer = peer; 
	blob_reset(&msg->buf); 
	blob_set_type(&msg->buf, BLOB_FIELD_TABLE); 
	blob_put_string(&msg->buf, "jsonrpc"); 
	blob_put_string(&msg->buf, "2.0"); 
	blob_put_string(&msg->buf, "id"); 
	blob_put_int(&msg->buf, seq); 
	blob_put_string(&msg->buf, "method"); 
	blob_put_string(&msg->buf, rpc_method); 
	blob_put_string(&msg->buf, "params"); 
	blob_offset_t arr = blob_open_array(&msg->buf); 
		blob_put_string(&msg->buf, object); 
		blob_put_string(&msg->buf, method); 
		blob_put_attr(&msg->buf, data); 
	blob_close_array(&msg->buf, arr); 

	if(ubus_socket_send(self->socket, &msg) < 0){
		printf("send failed!\n"); 
		return -1; 
	}
	return 0; 
}

static int _ubus_send_signal(struct ubus_context *self, uint32_t peer, const char *signal, struct blob_field *data){
	struct ubus_message *msg = ubus_socket_new_message(self->socket); 
	msg->peer = peer; 
	blob_reset(&msg->buf); 
	blob_set_type(&msg->buf, BLOB_FIELD_TABLE); 
	blob_put_string(&msg->buf, "jsonrpc"); 
	blob_put_string(&msg->buf, "2.0"); 
	blob_put_string(&msg->buf, "method"); 
	blob_put_string(&msg->buf, signal); 
	blob_put_string(&msg->buf, "params"); 
	blob_put_attr(&msg->buf, data); 

	if(ubus_socket_send(self->socket, &msg) < 0){
		printf("send signal failed\n"); 
		return -1; 
	}
	return 0; 
}
 
static void _send_well_known_name(struct ubus_context *self, uint32_t peer) {
	struct blob buf; 
	blob_init(&buf, 0, 0); 
	blob_put_string(&buf, self->name); 
	_ubus_send_signal(self, peer, "ubus.peer.well_known_name", blob_head(&buf));  
	blob_free(&buf); 
}

static void _on_reject_method_call(struct ubus_request *req, struct blob_field *msg){
	struct ubus_context *self = (struct ubus_context*)ubus_request_get_userdata(req); 

	// send reply with the same serial as the original request
	//printf("sending error to %08x\n", req->src_id); 

	_ubus_send_error(self, req->src_id, req->seq, msg); 
}

static void _on_msg_call(struct ubus_context *self, struct ubus_peer *peer, uint32_t serial, const char *rpc_method, struct blob_field *params){
	if(!self->root_obj) return; 

	struct blob buf; 
	blob_init(&buf, 0, 0); 

	// now we have to create a new request object which we bind to reply functions
	// so that when the application code calls ubus_request_resolve() we can 
	// send back the result over the network to the other peer

	struct ubus_request *req = ubus_request_new(peer->name, self->root_obj->name, rpc_method, params); 
	req->src_id = peer->id; 
	req->seq = serial; 
	ubus_request_set_userdata(req, self); 
	ubus_request_on_resolve(req, &_on_resolve_method_call); 
	ubus_request_on_reject(req, &_on_reject_method_call); 

	struct ubus_method *m = ubus_object_find_method(self->root_obj, rpc_method); 
	if(!m) {
		blob_put_int(&buf, UBUS_STATUS_METHOD_NOT_FOUND); 
		blob_put_string(&buf, "UBUS_STATUS_METHOD_NOT_FOUND"); 
		_ubus_send_error(self, peer->id, serial, blob_head(&buf)); 
		blob_free(&buf); 
		return; 
	}

	int ret = 0; 
	if((ret = ubus_method_invoke(m, self, self->root_obj, req, params)) < 0){
		ubus_request_reject(req, blob_head(&buf)); 
		ubus_request_delete(&req); 
	} else {
		// add the request to the list of pending requests 
		list_add(&req->list, &self->pending_incoming); 
	}
	blob_free(&buf); 
}

static void _on_msg_return(struct ubus_context *self, struct ubus_peer *peer, uint32_t serial, struct blob_field *msg){
	//printf("got return for request %d\n", serial); 
	struct ubus_request *req, *tmp, *found = NULL; 
	// find the pending outgoing request that has the same serial 
	list_for_each_entry_safe(req, tmp, &self->pending, list){
		if(req->seq == serial){
			found = req; 
			break; 
		}
	}
	if(!found) return; 
	list_del_init(&req->list); 
	ubus_request_resolve(req, msg); 
	ubus_request_delete(&req); 
}

static void _on_msg_error(struct ubus_context *self, struct ubus_peer *peer, uint32_t serial, struct blob_field *msg){
	struct ubus_request *req, *tmp, *found = NULL; 
	// find the pending outgoing request that has the same serial 
	list_for_each_entry_safe(req, tmp, &self->pending, list){
		if(req->seq == serial){
			found = req; 
			break; 
		}
	}
	if(!found) {
		//printf("request not found!\n"); 
		return; 
	}
	list_del_init(&req->list); 
	ubus_request_reject(req, msg); 
	ubus_request_delete(&req); 
}
/*
struct ubus_peer* _on_msg_client_connected(struct ubus_context *self, uint32_t peer_id){
	return _create_peer(self, peer_id); 
	//printf("peer connected id %08x\n", peer_id); 	
	// send out our name to peer
	//blob_reset(&self->buf); 
	//blob_put_string(&self->buf, "ubus.peer.well_known_name"); 
	//blob_put_string(&self->buf, self->name); 
	//TODO: send client well known name to the new peer
	//ubus_socket_send(self->socket, peer_id, UBUS_MSG_SIGNAL, self->request_seq++, blob_head(&self->buf)); 
}
*/
struct rpc_message {
	uint32_t id; 
	int type; 
	const char *method; 
	struct blob_field *params; 
	struct blob_field *error; 
	struct blob_field *result; 
}; 

static bool _parse_rpc_message(struct blob_field *field, struct rpc_message *self){
	struct blob_field *key, *value; 
	bool valid = false; 
	memset(self, 0, sizeof(*self)); 	

	blob_field_for_each_kv(field, key, value){
		const char *k = blob_field_get_string(key); 
		if(strcmp(k, "jsonrpc") == 0 && strcmp(blob_field_get_string(value), "2.0") == 0) valid = true; 
		else if(strcmp(k, "id") == 0) self->id = blob_field_get_int(value); 
		else if(strcmp(k, "method") == 0) self->method = blob_field_get_string(value); 
		else if(strcmp(k, "params") == 0) self->params = value; 
		else if(strcmp(k, "result") == 0) self->result = value; 
		else if(strcmp(k, "error") == 0) self->error = value;
	}
	if(!valid) return false; 
	if(self->id && self->method) self->type = UBUS_MSG_METHOD_CALL; 
	else if(!self->id && self->method) self->type = UBUS_MSG_SIGNAL; 
	else if(self->result) self->type = UBUS_MSG_METHOD_RETURN;  
	else if(self->error) self->type = UBUS_MSG_ERROR;  
	else return false; 
	return true; 
}

static void _ubus_handle_message(struct ubus_context *self, struct ubus_message *data){
	assert(self); 

	struct ubus_peer *p = _find_peer_by_id(self, data->peer);  
	if(!p){
		//printf("creating new peer context %08x\n", peer); 
		p = _create_peer(self, data->peer); 
	}

	// parse json message
	struct rpc_message msg; 
	if(!_parse_rpc_message(blob_head(&data->buf), &msg)){
		printf("could not parse rpc message: "); 
		blob_field_dump_json(blob_head(&data->buf)); 
		return;  	
	}

	switch(msg.type){
		case UBUS_MSG_METHOD_CALL: {
			if(!p) break; 
			_on_msg_call(self, p, msg.id, msg.method, msg.params); 
			break; 
		}
		case UBUS_MSG_METHOD_RETURN: {		
			if(!p) break; 
			_on_msg_return(self, p, msg.id, msg.result); 
			break; 
		}
		case UBUS_MSG_SIGNAL: {
			if(!p) break; 
			_on_msg_signal(self, p, msg.method, msg.params); 
			break; 
		}
		case UBUS_MSG_ERROR: {
			if(!p) break; 
			_on_msg_error(self, p, msg.id, msg.error); 
			break; 
		}
	}
}

void ubus_context_init(struct ubus_context *self, const char *name){
	INIT_LIST_HEAD(&self->requests); 
	INIT_LIST_HEAD(&self->pending); 
	INIT_LIST_HEAD(&self->pending_incoming); 
	avl_init(&self->peers_by_name, avl_intcmp, false, NULL); 
	avl_init(&self->peers_by_id, avl_intcmp, false, NULL); 
	//avl_init(&self->objects_by_name, avl_strcmp, false, NULL); 
	//avl_init(&self->objects_by_id, avl_intcmp, false, NULL); 
	self->socket = ubus_socket_new();  
	blob_init(&self->buf, 0, 0); 
	self->name = strdup(name);
	self->request_seq = 1; 
}

void ubus_context_destroy(struct ubus_context *self){
	// delete all requests
	//printf("delete context %s\n", self->name); 
	struct ubus_request *req, *tmp; 
	list_for_each_entry_safe(req, tmp, &self->requests, list){
		//ubus_request_reject(req, NULL); 
		ubus_request_delete(&req); 
	}
	list_for_each_entry_safe(req, tmp, &self->pending, list){
		//ubus_request_reject(req, NULL); 
		ubus_request_delete(&req); 
	}
	list_for_each_entry_safe(req, tmp, &self->pending_incoming, list){
		//ubus_request_reject(req, NULL); 
		ubus_request_delete(&req); 
	}

	// remove all peers
	struct ubus_peer *peer = 0, *ptr; 
	avl_for_each_element_safe(&self->peers_by_id, peer, avl_id, ptr){
		ubus_peer_delete(&peer); 
	}
	
	// remove all objects
	//struct ubus_object *obj, *optr; 
	//avl_for_each_element_safe(&self->objects_by_name, obj, avl, optr){
//		ubus_object_delete(&obj); 
	//}
	// objects_by_id does not need to be freed!

	ubus_socket_delete(&self->socket); 
	blob_free(&self->buf); 
	free(self->name); 
}

struct ubus_context *ubus_new(const char *name, struct ubus_object **root){
	struct ubus_context *self = calloc(1, sizeof(*self)); 
	ubus_context_init(self, name); 
	if(root) { self->root_obj = *root; *root = 0; } 
	else self->root_obj = ubus_object_new("root"); 
	return self; 
}

void ubus_delete(struct ubus_context **self){
	assert(self); 
	ubus_context_destroy(*self); 
	ubus_object_delete(&(*self)->root_obj); 
	free(*self); 
	*self = NULL; 
}

int ubus_connect(struct ubus_context *self, const char *path, uint32_t *peer_id){
	if(!path) path = UBUS_DEFAULT_SOCKET; 
	uint32_t peer = 0; 
	int ret = ubus_socket_connect(self->socket, path, &peer); 
	if(ret < 0) return ret; 

	_send_well_known_name(self, peer);

	if(peer_id) *peer_id = peer; 
	return 0; 
}

int ubus_listen(struct ubus_context *self, const char *path){
	return ubus_socket_listen(self->socket, path); 	
}

int ubus_set_peer_localname(struct ubus_context *self, uint32_t peer_id, const char *localname){
	struct ubus_peer *peer = _find_peer_by_name(self, localname); 
	if(peer) return -1; 
	peer = _find_peer_by_id(self, peer_id);  
	if(!peer) return -1; 
	if(peer->avl_name.key && strlen(peer->avl_name.key) > 0) 
		avl_delete(&self->peers_by_name, &peer->avl_name); 
	ubus_peer_set_name(peer, localname); 
	return avl_insert(&self->peers_by_name, &peer->avl_name); 
}

static void _ubus_send_pending(struct ubus_context *self){
	struct ubus_request *req, *tmp; 	
	list_for_each_entry_safe(req, tmp, &self->requests, list){
		if(utick_expired(req->timeout)){
			printf("request timed out to %s %s!\n", req->object, req->method); 
			list_del_init(&req->list); 
			ubus_request_reject(req, blob_head(&self->buf)); 
			ubus_request_delete(&req); 
			continue; 
		}
		// see if we have the target client
		struct ubus_peer *peer = _find_peer_by_name(self, req->dst_name); 

		if(!peer) continue; 
		//printf("found peer for request %s %08x\n", req->dst_name, peer->id); 

		if(_ubus_send_request(self, peer->id, req->seq, "call", req->object, req->method, blob_head(&req->buf)) < 0){
			printf("request failed to %s %s!\n", req->object, req->method); 
			list_del_init(&req->list); 
			ubus_request_reject(req, blob_head(&self->buf)); 
			ubus_request_delete(&req); 
			continue; 
		}

		//blob_put_string(&self->buf, ""); 

		// move the request to pending queue
		list_del_init(&req->list); 
		req->dst_id = peer->id; 
		list_add(&req->list, &self->pending); 
	}
}

int ubus_send_request(struct ubus_context *self, struct ubus_request **_req){
	struct ubus_request *req = *_req; 
	req->seq = self->request_seq++; 
	//printf("sending request %08x\n", req->seq); 
	req->timeout = utick_now() + 5000000UL; 
	list_add(&req->list, &self->requests); 
	_ubus_send_pending(self); 

	return 0; 
}
/*
uint32_t ubus_add_object(struct ubus_context *self, struct ubus_object **_obj){
	// add the object to our local list of objects and tell all peers that we have this object
	struct ubus_object *obj = *_obj; 
	*_obj = NULL;

	if(avl_insert(&self->objects_by_name, &obj->avl) != 0){
		ubus_object_delete(&obj); 
		return -1; 
	}

	ubus_id_alloc(&self->objects_by_id, &obj->id, 0); 

	return obj->id.id; 
}
*/
int ubus_handle_events(struct ubus_context *self){
	// try to send out pending requests
	struct ubus_request *req; 
	struct ubus_request *tmp; 

	// check if any of the pending requests has timed out
	list_for_each_entry_safe(req, tmp, &self->pending, list){
		if(utick_expired(req->timeout)){
			printf("pending request timed out! %s %s\n", req->object, req->method); 
			list_del_init(&req->list); 
			ubus_request_reject(req, blob_head(&self->buf)); 
			ubus_request_delete(&req); 
		}
	}

	list_for_each_entry_safe(req, tmp, &self->pending_incoming, list){
		if(req->failed || req->resolved){
			//printf("deleting completed request\n");
			list_del_init(&req->list); 
			ubus_request_delete(&req); 
		}
	}
	
	_ubus_send_pending(self); 

	// try reading a message
	struct ubus_message *msg; 
	while(ubus_socket_recv(self->socket, &msg) > 0){
		_ubus_handle_message(self, msg); 
	}
	return 0; 
}

const char *ubus_status_to_string(int8_t status){
	static const char *code[] = {
		"UBUS_STATUS_OK",
		"UBUS_STATUS_INVALID_COMMAND",
		"UBUS_STATUS_INVALID_ARGUMENT",
		"UBUS_STATUS_METHOD_NOT_FOUND",
		"UBUS_STATUS_NOT_FOUND",
		"UBUS_STATUS_NO_DATA",
		"UBUS_STATUS_PERMISSION_DENIED",
		"UBUS_STATUS_TIMEOUT",
		"UBUS_STATUS_NOT_SUPPORTED",
		"UBUS_STATUS_UNKNOWN_ERROR",
		"UBUS_STATUS_CONNECTION_FAILED"
	}; 
	if(status < 0 || status > sizeof(code) / sizeof(code[0])) status = UBUS_STATUS_UNKNOWN_ERROR; 
	return code[status]; 
}

