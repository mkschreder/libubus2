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
#include "ubus_socket.h"
#include "ubus_rawsocket.h"
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

struct ubus_object *_find_object_by_name(struct ubus_context *self, const char *path){
	struct avl_node *avl = avl_find(&self->objects_by_name, path); 
	if(!avl) return NULL; 
	return container_of(avl, struct ubus_object, avl); 
}

void _on_msg_signal(struct ubus_context *self, struct ubus_peer *peer, uint32_t serial, struct blob_field *msg){
	// first argument is always signal type
	struct blob_field *attr = blob_field_first_child(msg); 
	const char *signal_name = blob_field_get_string(attr); 
	if(!signal_name) return; 

	if(strcmp(signal_name, "ubus.peer.well_known_name") == 0){
		attr = blob_field_next_child(msg, attr); 
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
	} 
}

static void _on_resolve_method_call(struct ubus_request *req, struct blob_field *msg){
	struct ubus_context *self = (struct ubus_context*)ubus_request_get_userdata(req); 

	// send reply with the same serial as the original request
	//printf("sending response to %08x\n", req->src_id); 

	ubus_socket_send(self->socket, req->src_id, UBUS_MSG_METHOD_RETURN, req->seq, msg);  
}

static void _on_reject_method_call(struct ubus_request *req, struct blob_field *msg){
	struct ubus_context *self = (struct ubus_context*)ubus_request_get_userdata(req); 

	// send reply with the same serial as the original request
	//printf("sending error to %08x\n", req->src_id); 
	
	blob_reset(&self->buf); 
	struct blob_field *cur; 
	blob_field_for_each_child(msg, cur){
		blob_put_attr(&self->buf, cur); 
	}
	ubus_socket_send(self->socket, req->src_id, UBUS_MSG_ERROR, req->seq, blob_head(&self->buf));  
}

static void _on_msg_call(struct ubus_context *self, struct ubus_peer *peer, uint32_t serial, struct blob_field *msg){
	struct blob buf; 
	blob_init(&buf, 0, 0); 
	
	// arg 2: object path
	// arg 3: method name
	// arg 4: arguments
	struct blob_field *attr = blob_field_first_child(msg); 
	const char *object = blob_field_get_string(attr); 
	attr = blob_field_next_child(msg, attr); 
	const char *method = blob_field_get_string(attr); 
	attr = blob_field_next_child(msg, attr); 

	// find the object being refered to in our local list 
	struct ubus_object *obj = _find_object_by_name(self, object); 
	if(!obj){
		//printf("object %s not found!\n", object); 
		blob_put_int(&buf, UBUS_STATUS_NOT_FOUND); 
		goto reject;
	}

	struct ubus_method *m = ubus_object_find_method(obj, method); 
	if(!m) {
		//printf("method %s not found on %s\n", method, object); 
		blob_put_int(&buf, UBUS_STATUS_METHOD_NOT_FOUND); 
		goto reject;
	}

	// now we have to create a new request object which we bind to reply functions
	// so that when the application code calls ubus_request_resolve() we can 
	// send back the result over the network to the other peer

	struct ubus_request *req = ubus_request_new(peer->name, object, method, msg); 
	req->src_id = peer->id; 
	req->seq = serial; 
	ubus_request_set_userdata(req, self); 
	ubus_request_on_resolve(req, &_on_resolve_method_call); 
	ubus_request_on_reject(req, &_on_reject_method_call); 

	int ret = 0; 
	if((ret = ubus_method_invoke(m, self, obj, req, attr)) != 0){
		blob_reset(&buf); 
		blob_put_int(&buf, ret); 
		blob_put_string(&buf, object); 
		blob_put_string(&buf, method); 
		ubus_request_reject(req, blob_head(&buf)); 
		ubus_request_delete(&req); 
	} else {
		// add the request to the list of pending requests 
		list_add(&req->list, &self->pending_incoming); 
	}
	goto free; 
reject: 
	//ubus_request_reject(req, blob_head(&buf)); 
	ubus_socket_send(self->socket, peer->id, UBUS_MSG_ERROR, serial, blob_head(&buf));  
free: 
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

void _on_msg_client_connected(struct ubus_context *self, uint32_t peer_id){
	_create_peer(self, peer_id); 
	//printf("peer connected id %08x\n", peer_id); 	
	// send out our name to peer
	blob_reset(&self->buf); 
	blob_put_string(&self->buf, "ubus.peer.well_known_name"); 
	blob_put_string(&self->buf, self->name); 
	ubus_socket_send(self->socket, peer_id, UBUS_MSG_SIGNAL, self->request_seq++, blob_head(&self->buf)); 
}

static void _on_message_received(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg){
	struct ubus_context *self = (struct ubus_context*)ubus_socket_get_userdata(socket);  
	assert(self); 

	struct ubus_peer *p = _find_peer_by_id(self, peer);  

	switch(type){
		case UBUS_MSG_METHOD_CALL: {
			if(!p) break; 
			_on_msg_call(self, p, serial, msg); 
			break; 
		}
		case UBUS_MSG_METHOD_RETURN: {		
			if(!p) break; 
			_on_msg_return(self, p, serial, msg); 
			break; 
		}
		case UBUS_MSG_SIGNAL: {
			if(!p) break; 
			_on_msg_signal(self, p, serial, msg); 
			break; 
		}
		case UBUS_MSG_ERROR: {
			if(!p) break; 
			_on_msg_error(self, p, serial, msg); 
			break; 
		}
		case UBUS_MSG_PEER_CONNECTED: {
			_on_msg_client_connected(self, peer); 
			break; 
		}
	}
}

void ubus_context_init(struct ubus_context *self, const char *name, ubus_socket_t *socket){
	INIT_LIST_HEAD(&self->requests); 
	INIT_LIST_HEAD(&self->pending); 
	INIT_LIST_HEAD(&self->pending_incoming); 
	avl_init(&self->peers_by_name, avl_intcmp, false, NULL); 
	avl_init(&self->peers_by_id, avl_intcmp, false, NULL); 
	avl_init(&self->objects_by_name, avl_strcmp, false, NULL); 
	avl_init(&self->objects_by_id, avl_intcmp, false, NULL); 
	if(socket) { self->socket = *socket; *socket = NULL; }
	else self->socket = ubus_rawsocket_new();  
	ubus_socket_set_userdata(self->socket, self); 
	ubus_socket_on_message(self->socket, &_on_message_received); 
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
	struct ubus_object *obj, *optr; 
	avl_for_each_element_safe(&self->objects_by_name, obj, avl, optr){
		ubus_object_delete(&obj); 
	}
	// objects_by_id does not need to be freed!

	ubus_socket_delete(self->socket); 
	blob_free(&self->buf); 
	free(self->name); 
}

struct ubus_context *ubus_new(const char *name, ubus_socket_t *socket){
	struct ubus_context *self = calloc(1, sizeof(*self)); 
	ubus_context_init(self, name, socket); 
	return self; 
}

void ubus_delete(struct ubus_context **self){
	assert(self); 
	ubus_context_destroy(*self); 
	free(*self); 
	*self = NULL; 
}

int ubus_connect(struct ubus_context *self, const char *path, uint32_t *peer_id){
	if(!path) path = UBUS_DEFAULT_SOCKET; 
	return ubus_socket_connect(self->socket, path, peer_id); 
}

int ubus_listen(struct ubus_context *self, const char *path){
	return ubus_socket_listen(self->socket, path); 	
}

int ubus_set_peer_localname(struct ubus_context *self, uint32_t peer_id, const char *localname){
	struct ubus_peer *peer = _find_peer_by_id(self, peer_id);  
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
		blob_reset(&self->buf); 

		struct ubus_peer *peer = _find_peer_by_name(self, req->dst_name); 

		if(!peer) continue; 
		
		//printf("found peer for request %s %08x\n", req->dst_name, peer->id); 

		//blob_put_string(&self->buf, ""); 
		blob_put_string(&self->buf, req->object); 
		blob_put_string(&self->buf, req->method); 
		blob_put_attr(&self->buf, blob_head(&req->buf)); 
		ubus_socket_send(self->socket, peer->id, UBUS_MSG_METHOD_CALL, req->seq, blob_head(&self->buf)); 

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
			continue; 
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

	// poll for incoming events
	ubus_socket_handle_events(self->socket, 0); 
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

