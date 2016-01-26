/*
 * Copyright (C) 2016 Martin K. Schröder <mkschreder.uk@gmail.com>
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
#include "ubus_server.h"
#include <blobpack/blobpack.h>
#include <libutype/avl-cmp.h>
#include <pthread.h>

struct ubus_forward_info {
	struct list_head list; 
	uint32_t attached_id; 
	char *client; 
	char *object_name; 
	struct ubus_object *object; 
}; 

struct ubus_forward_info *forward_info_new(void){
	struct ubus_forward_info *self = calloc(1, sizeof(struct ubus_forward_info)); 
	INIT_LIST_HEAD(&self->list); 
	return self; 
}
/*
static uint32_t _ubus_server_add_object(struct ubus_server *self, struct ubus_object **_obj){
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
void _on_forward_response(struct ubus_request *req, struct blob_field *res){
	printf("forwarded request succeeded!\n"); 
	struct ubus_request *or = (struct ubus_request*)ubus_request_get_userdata(req); 
	ubus_request_resolve(or, res); 
}

void _on_forward_failed(struct ubus_request *req, struct blob_field *res){
	printf("ERROR request failed!\n"); 
	struct ubus_request *or = (struct ubus_request*)ubus_request_get_userdata(req); 
	ubus_request_reject(or, res); 
}

struct ubus_object *_find_object(struct ubus_server *self, const char *path){
	struct ubus_forward_info *info; 
	list_for_each_entry(info, &self->objects, list){
		if(strcmp(info->object->name, path) == 0) return info->object; 
	}
	return NULL; 
}

static int _on_forward_call(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_field *msg){
	struct ubus_forward_info *info = ubus_object_get_userdata(obj); 
	assert(info); 

	printf("forward call from %s to %s %s\n", req->dst_name, info->client, info->object_name); 
	struct ubus_request *r = ubus_request_new(info->client, info->object_name, self->name, msg); 
	ubus_request_on_resolve(r, &_on_forward_response); 
	ubus_request_on_reject(r, &_on_forward_failed); 
	ubus_request_set_userdata(r, req); 
	ubus_send_request(ctx, &r); 

	return 0; 
}

static int _on_nick(struct ubus_method *_method, struct ubus_context *ctx, struct ubus_object *_obj, struct ubus_request *req, struct blob_field *msg){
	//struct ubus_server *self = (struct ubus_server*)ubus_get_userdata(ctx); 

	// arg 0: nick
	// arg 1: username
	// arg 3: password

	struct blob_field *attr = blob_field_first_child(msg); 
	const char *peer_name = blob_field_get_string(attr); 	
	
	printf("setting localname of %08x to %s\n", req->src_id, peer_name); 
	ubus_set_peer_localname(ctx, req->src_id, peer_name); 
	
	ubus_request_resolve(req, NULL); 
	return 0; 
}

static int _on_call(struct ubus_method *_method, struct ubus_context *ctx, struct ubus_object *_obj, struct ubus_request *req, struct blob_field *msg){
	struct ubus_server *self = (struct ubus_server*)ubus_get_userdata(ctx); 

	// arg 2: object path
	// arg 3: method name
	// arg 4: arguments
	struct blob_field *attr = blob_field_first_child(msg); 
	const char *object = blob_field_get_string(attr); 
	attr = blob_field_next_child(msg, attr); 
	const char *method = blob_field_get_string(attr); 
	attr = blob_field_next_child(msg, attr); 

	struct blob buf; 
	blob_init(&buf, 0, 0); 

	// find the object being refered to in our local list 
	struct ubus_object *obj = _find_object(self, object); 
	if(!obj){
		blob_free(&buf); 
		return UBUS_STATUS_NOT_FOUND; 
	}
		
	struct ubus_method *m = ubus_object_find_method(obj, method); 
	if(!m) {
		//printf("method %s not found on %s\n", method, object); 
		blob_put_int(&buf, UBUS_STATUS_METHOD_NOT_FOUND); 
		ubus_request_reject(req, blob_head(&buf)); 
		blob_free(&buf); 
		return 0; 
	}
	
	int ret; 
	if((ret = ubus_method_invoke(m, ctx, obj, req, attr)) != 0){
		blob_reset(&buf); 
		blob_put_int(&buf, ret); 
		blob_put_string(&buf, ubus_status_to_string(ret));  

		ubus_request_reject(req, blob_head(&buf)); 
	}

	blob_free(&buf); 

	return 0; 
}

static int _on_list_objects(struct ubus_method *m, struct ubus_context *ctx, struct ubus_object *_obj, struct ubus_request *req, struct blob_field *msg){
	struct ubus_server *self = (struct ubus_server*)ubus_get_userdata(ctx); 
	struct blob buf; 
	blob_init(&buf, 0, 0); 

	//struct ubus_object *obj = NULL; 
	struct ubus_forward_info *cur; 
	blob_offset_t tbl = blob_open_table(&buf); 
	list_for_each_entry(cur, &self->objects, list){
		blob_put_string(&buf, cur->object->name); 
		ubus_object_serialize(cur->object, &buf); 
	}
	blob_close_table(&buf, tbl); 
	/*avl_for_each_element(&self->objects_by_name, obj, avl){
		//printf("got object %s\n", obj->name); 
		blob_put_string(&buf, obj->name); 
		ubus_object_serialize(obj, &buf); 
	}*/

	ubus_request_resolve(req, blob_head(&buf)); 	
	blob_free(&buf); 

	return 0; 
}

static int _on_publish_object(struct ubus_method *m, struct ubus_context *ctx, struct ubus_object *_obj, struct ubus_request *req, struct blob_field *msg){
	assert(msg); 

	printf("publish object: "); 
	blob_field_dump_json(msg); 

	struct ubus_server *self = ubus_get_userdata(ctx); 
	char path[255]; 
	struct blob_field *params[3]; 

	msg = blob_field_first_child(msg); 

	if(!blob_field_parse(msg, "s{sa}", params, 2)) {
		return UBUS_STATUS_INVALID_ARGUMENT; 
	}
	const char *objname = blob_field_get_string(params[0]); 	

	//blob_field_dump_json(msg); 
	snprintf(path, sizeof(path), "%s.%s", req->dst_name, objname); 

	printf("publishing %s\n", path); 
	// create proxy object for the published object
	struct ubus_object *obj = ubus_object_new(path); 
	struct blob_field *mname, *margs; 
	blob_field_for_each_kv(params[1], mname, margs){
		struct ubus_method *method = ubus_method_new(blob_field_get_string(mname), _on_forward_call); 
		struct blob_field *arg; 
		// copy over each argument 
		blob_field_for_each_child(margs, arg){ 
			blob_put_attr(&method->signature, arg); 
		}
		ubus_object_add_method(obj, &method); 
	}

	struct ubus_forward_info *info = forward_info_new(); 
	info->client = strdup(req->dst_name); 
	info->object_name = strdup(objname); 
	info->object = obj; 
	ubus_object_set_userdata(obj, info); 

	//_ubus_server_add_object(self, &obj); 
	//info->attached_id = ubus_add_object(ctx, &obj); 

	list_add(&info->list, &self->objects); 

	ubus_request_resolve(req, NULL); 
	//printf("object published!\n"); 

	return 0; 
}

int ubus_server_send(struct ubus_server *self, struct ubus_message **msg){
	struct ubus_id *id;  
	if(peer == UBUS_PEER_BROADCAST){
		avl_for_each_element(&self->clients, id, avl){
			struct ubus_rawsocket_client *client = (struct ubus_rawsocket_client*)container_of(id, struct ubus_rawsocket_client, id);  
			struct ubus_frame *req = ubus_frame_new(msg);
			list_add(&req->list, &client->tx_queue); 
			// try to send as much as we can right away
			_ubus_rawsocket_client_send(client); 
			//printf("added request to tx_queue!\n"); 
		}		
	} else {
		struct ubus_id *id = ubus_id_find(&self->clients, peer); 
		if(!id) return -1; 
		struct ubus_rawsocket_client *client = (struct ubus_rawsocket_client*)container_of(id, struct ubus_rawsocket_client, id);  
		struct ubus_frame *req = ubus_frame_new(msg);
		list_add(&req->list, &client->tx_queue); 
		_ubus_rawsocket_client_send(client); 
	}
}

struct ubus_server *ubus_server_new(const char *name){
	struct ubus_server *self = calloc(1, sizeof(struct ubus_server)); 

	ubus_id_tree_init(&self->clients); 

	struct ubus_object *obj = ubus_object_new("root"); 
	struct ubus_method *method = ubus_method_new("publish", _on_publish_object); 
	ubus_method_add_param(method, "name", "s"); 
	ubus_method_add_param(method, "signature", "[sa]"); 
	ubus_object_add_method(obj, &method); 

	method = ubus_method_new("call", _on_call); 
	ubus_object_add_method(obj, &method); 
	ubus_object_set_userdata(obj, self); 

	method = ubus_method_new("nick", _on_nick); 
	ubus_object_add_method(obj, &method); 
	ubus_object_set_userdata(obj, self); 

	method = ubus_method_new("list", _on_list_objects); 
	ubus_object_add_method(obj, &method); 
	ubus_object_set_userdata(obj, self); 
	
	self->ctx = ubus_new(name, &obj); 
	INIT_LIST_HEAD(&self->objects); 

	ubus_set_userdata(self->ctx, self); 

	pthread_create(&self->thread, NULL, _server_thread, self); 
	return self; 
}

void ubus_server_delete(struct ubus_server **self){
	(*self)->shutdown = true; 
	pthread_join(&self->thread); 

	struct ubus_forward_info *info, *tmp; 
	// free the info objects but we can only do this like this because we also free objects
	// otherwise we would have to look up each object as well and set userdata to null!
	list_for_each_entry_safe(info, tmp, &(*self)->objects, list){
		free(info->client); 
		free(info->object); 
		free(info); 
	}

	struct ubus_id *id, *tmp; 
	avl_for_each_element_safe(&self->clients, id, avl, tmp){
		struct ubus_rawsocket_client *client = container_of(id, struct ubus_rawsocket_client, id);  
		ubus_id_free(&self->clients, &client->id); 
		ubus_rawsocket_client_delete(&client); 
	}

	ubus_delete(&(*self)->ctx); 
	free(*self); 
	*self = NULL; 
}

int ubus_server_listen(struct ubus_server *self, const char *path){
	char proto[MAX_PATH] = {0}, host[MAX_PATH] = {0}, path[MAX_PATH] = {0};
	int port = 5303;  
	if(!_scan_url(path, proto, host, &port, path)) return -1; 	

	return 0; 
}


int ubus_server_connect(struct ubus_server *self, const char *url){
	struct ubus_rawsocket *self = container_of(socket, struct ubus_rawsocket, api); 
	char proto[MAX_PATH] = {0}, host[MAX_PATH] = {0}, path[MAX_PATH] = {0};
	int port = 5303;  
	int flags = 0; 
	if(!_scan_url(path, proto, host, &port, path)) return -1; 	

	if(host[0] == '/' || (host[0] == '.' && host[1] == '/')) flags |= USOCK_UNIX;

	// TODO: connect the implementaiton server here. 
	return 0; 
}

static int _ubus_server_handle_events(struct ubus_server *self, int timeout){
	struct ubus_rawsocket *self = container_of(socket, struct ubus_rawsocket, api); 
	int count = avl_size(&self->clients) + 1; 
	struct pollfd *pfd = alloca(sizeof(struct pollfd) * count); 
	memset(pfd, 0, sizeof(struct pollfd) * count); 
	struct ubus_rawsocket_client **clients = alloca(sizeof(void*)*100); 
	pfd[0] = (struct pollfd){ .fd = self->listen_fd, .events = POLLIN | POLLERR }; 
	clients[0] = 0; 

	int c = 1; 
	struct ubus_id *id, *tmp;  
	avl_for_each_element(&self->clients, id, avl){
		struct ubus_rawsocket_client *client = (struct ubus_rawsocket_client*)container_of(id, struct ubus_rawsocket_client, id);  
		pfd[c] = (struct pollfd){ .fd = client->fd, .events = POLLOUT | POLLIN | POLLERR };  
		clients[c] = client;  
		c++; 
	}		
	
	// try to send more data
	for(int c = 1; c < count; c++){
		_ubus_rawsocket_client_send(clients[c]); 
	}

	int ret = 0; 
	if((ret = poll(pfd, count, timeout)) > 0){
		if(pfd[0].revents != 0){
			// TODO: check for errors
			_accept_connection(self);
		}
		for(int c = 1; c < count; c++){
			if(pfd[c].revents != 0){
				if(pfd[c].revents & POLLHUP || pfd[c].revents & POLLRDHUP){
					//printf("ERROR: peer hung up!\n"); 
					_ubus_rawsocket_client_recv(clients[c], self); 
					avl_delete(&self->clients, &clients[c]->id.avl); 
					ubus_rawsocket_client_delete(&clients[c]); 
					continue; 
				} else if(pfd[c].revents & POLLERR){
					printf("ERROR: socket error!\n"); 
				} else if(pfd[c].revents & POLLIN) {
					// receive as much data as we can
					_ubus_rawsocket_client_recv(clients[c], self); 
				}
			}
		}
	}

	// remove any disconnected clients
	avl_for_each_element_safe(&self->clients, id, avl, tmp){
		struct ubus_rawsocket_client *client = (struct ubus_rawsocket_client*)container_of(id, struct ubus_rawsocket_client, id);  
		if(client->disconnected) {
			_rawsocket_remove_client(self, &client); 
			continue; 
		}
	}
}


