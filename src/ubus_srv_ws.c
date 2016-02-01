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

#include <blobpack/blobpack.h>

#define _GNU_SOURCE
#include "ubus_srv_ws.h"
#include "../src/ubus_message.h"
#include "../src/ubus_srv.h"
#include "../src/ubus_id.h"
#include "mimetypes.h"
#include <libutype/list.h>
#include <libutype/avl.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <assert.h>
#include <limits.h>

#include <libusys/usock.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "internal.h"

struct lws_context; 
struct ubus_srv_ws {
	struct lws_context *ctx; 
	struct lws_protocols *protocols; 
	struct avl_tree clients; 
	//struct blob buf; 
	const struct ubus_server_api *api; 
	bool shutdown; 
	pthread_t thread; 
	pthread_mutex_t qlock; 
	pthread_cond_t rx_ready; 
	struct list_head rx_queue; 
	const char *www_root; 
	void *user_data; 
}; 

struct ubus_srv_ws_client {
	struct ubus_id id; 
	struct list_head tx_queue; 
	struct ubus_message *msg; // incoming message
	bool disconnect;
}; 

struct ubus_srv_ws_frame {
	struct list_head list; 
	uint8_t *buf; 
	int len; 
	int sent_count; 
}; 

struct ubus_srv_ws_frame *ubus_srv_ws_frame_new(struct blob_field *msg){
	assert(msg); 
	struct ubus_srv_ws_frame *self = calloc(1, sizeof(struct ubus_srv_ws_frame)); 
	INIT_LIST_HEAD(&self->list); 
	char *json = blob_field_to_json(msg); 
	//printf("frame: %s\n", json); 
	self->len = strlen(json); 
	self->buf = calloc(1, LWS_SEND_BUFFER_PRE_PADDING + self->len + LWS_SEND_BUFFER_POST_PADDING); 
	memcpy(self->buf + LWS_SEND_BUFFER_PRE_PADDING, json, self->len); 
	free(json); 
	self->sent_count = 0; 
	return self; 
}

void ubus_srv_ws_frame_delete(struct ubus_srv_ws_frame **self){
	assert(self && *self); 
	free((*self)->buf); 
	free(*self); 
	*self = NULL; 
}


static struct ubus_srv_ws_client *ubus_srv_ws_client_new(){
	struct ubus_srv_ws_client *self = calloc(1, sizeof(struct ubus_srv_ws_client)); 
	INIT_LIST_HEAD(&self->tx_queue); 
	self->msg = ubus_message_new(); 
	return self; 
}

static __attribute__((unused)) void ubus_srv_ws_client_delete(struct ubus_srv_ws_client **self){
	// TODO: free tx_queue
	struct ubus_srv_ws_frame *pos, *tmp; 
	list_for_each_entry_safe(pos, tmp, &(*self)->tx_queue, list){
		ubus_srv_ws_frame_delete(&pos);  
	}	
	ubus_message_delete(&(*self)->msg); 
	free(*self); 
	*self = NULL;
}

static int _ubus_socket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *_user, void *in, size_t len){
	// TODO: keeping user data in protocol is probably not the right place. Fix it. 
	const struct lws_protocols *proto = lws_get_protocol(wsi); 

	struct ubus_srv_ws_client **user = (struct ubus_srv_ws_client **)_user; 
	
	if(user && *user && (*user)->disconnect) return -1; 

	int32_t peer_id = lws_get_socket_fd(wsi); 
	switch(reason){
		case LWS_CALLBACK_ESTABLISHED: {
			struct ubus_srv_ws *self = (struct ubus_srv_ws*)proto->user; 
			struct ubus_srv_ws_client *client = ubus_srv_ws_client_new(lws_get_socket_fd(wsi)); 
			ubus_id_alloc(&self->clients, &client->id, 0); 
			*user = client; 
			char hostname[255], ipaddr[255]; 
			lws_get_peer_addresses(wsi, peer_id, hostname, sizeof(hostname), ipaddr, sizeof(ipaddr)); 
			printf("connection established! %s %s %d %08x\n", hostname, ipaddr, peer_id, client->id.id); 
			//if(self->on_message) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_PEER_CONNECTED, 0, NULL); 
			lws_callback_on_writable(wsi); 	
			break; 
		}
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("websocket: client error\n"); 
			break; 
		case LWS_CALLBACK_CLOSED: {
			printf("websocket: client disconnected %p %p\n", _user, *user); 
			struct ubus_srv_ws *self = (struct ubus_srv_ws*)proto->user; 
			//if(self->on_message) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_PEER_DISCONNECTED, 0, NULL); 
			ubus_id_free(&self->clients, &(*user)->id); 
			ubus_srv_ws_client_delete(user); 	
			*user = 0; 
			break; 
		}
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			if(list_empty(&(*user)->tx_queue)){
				lws_callback_on_writable(wsi); 	
				break; 
			}
			// TODO: handle partial writes correctly 
			struct ubus_srv_ws_frame *frame = list_first_entry(&(*user)->tx_queue, struct ubus_srv_ws_frame, list);
			int n = lws_write(wsi, &frame->buf[LWS_SEND_BUFFER_PRE_PADDING], frame->len, LWS_WRITE_TEXT);// | LWS_WRITE_NO_FIN);
			if(n < 0) return -1; 
			//printf("wrote %d bytes of %d\n", n, frame->len); 
			frame->sent_count += n; 
			if(frame->sent_count >= frame->len){
				list_del_init(&frame->list); 
				ubus_srv_ws_frame_delete(&frame); 
			}
			lws_callback_on_writable(wsi); 	
			//lws_rx_flow_control(wsi, 1); 
			break; 
		}
		case LWS_CALLBACK_RECEIVE: {
			assert(proto); 
			assert(user); 
			if(!user) break; 
			struct ubus_srv_ws *self = (struct ubus_srv_ws*)proto->user; 
			blob_reset(&(*user)->msg->buf); 
			if(blob_put_json(&(*user)->msg->buf, in)){
				//struct blob_field *rpcobj = blob_field_first_child(blob_head(&self->buf)); 
				//TODO: add message to queue
				printf("websocket message\n"); 
				pthread_mutex_lock(&self->qlock); 
				(*user)->msg->peer = (*user)->id.id; 
				list_add(&(*user)->msg->list, &self->rx_queue); 
				(*user)->msg = ubus_message_new(); 
				pthread_mutex_unlock(&self->qlock); 
				pthread_cond_signal(&self->rx_ready); 
			}
			//lws_rx_flow_control(wsi, 0); 
			lws_callback_on_writable(wsi); 	
			break; 
		}
		
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			printf("Client connected!\n"); 
			// TODO: implement once we support outgoing websocket connections
			break;
		case LWS_CALLBACK_HTTP: {
			struct ubus_srv_ws *self = (struct ubus_srv_ws*)proto->user; 
            char *requested_uri = (char *) in;
            printf("requested URI: %s\n", requested_uri);
           
            if (strcmp(requested_uri, "/") == 0) 
				requested_uri = "/index.html"; 

			// allocate enough memory for the resource path
			char *resource_path = malloc(strlen(self->www_root) + strlen(requested_uri) + 1);
		   
			// join current working direcotry to the resource path
			sprintf(resource_path, "%s%s", self->www_root, requested_uri);
			printf("resource path: %s\n", resource_path);
		   
			char *extension = strrchr(resource_path, '.');
		  	const char *mime = mimetype_lookup(extension); 
		   
			// by default non existing resources return code 404
			lws_serve_http_file(wsi, resource_path, mime, NULL, 0);
			// return 1 so that the connection shall be closed
			return 1; 
       	} break;     
            //lws_close_and_free_session(context, wsi,                                   LWS_CLOSE_STATUS_NORMAL);
		default: 
			break; 
	}; 

	return 0; 
}

void _websocket_destroy(ubus_server_t socket){
	struct ubus_srv_ws *self = container_of(socket, struct ubus_srv_ws, api); 
	self->shutdown = true; 
	printf("joining..\n"); 
	pthread_join(self->thread, NULL); 
	pthread_mutex_destroy(&self->qlock); 
	pthread_cond_destroy(&self->rx_ready); 
	struct ubus_id *id, *tmp; 
	avl_for_each_element_safe(&self->clients, id, avl, tmp){
		struct ubus_srv_ws_client *client = container_of(id, struct ubus_srv_ws_client, id);  
		ubus_id_free(&self->clients, &client->id); 
		ubus_srv_ws_client_delete(&client); 
	}

	if(self->ctx) lws_context_destroy(self->ctx); 
	printf("context destroyed\n"); 
	free(self->protocols); 
	free(self);  
}

int _websocket_listen(ubus_server_t socket, const char *path){
	struct ubus_srv_ws *self = container_of(socket, struct ubus_srv_ws, api); 
	struct lws_context_creation_info info; 
	memset(&info, 0, sizeof(info)); 

	char proto[NAME_MAX], host[NAME_MAX], file[NAME_MAX]; 
	int port = 5303; 
	if(!url_scanf(path, proto, host, &port, file)){
		fprintf(stderr, "Could not parse url: %s\n", path); 
		return -1; 
	}

	info.port = port;
	info.gid = -1; 
	info.uid = -1; 
	info.user = self; 
	info.protocols = self->protocols; 
	info.extensions = lws_get_internal_extensions();
	info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;

	self->ctx = lws_create_context(&info); 

	return 0; 
}

int _websocket_connect(ubus_server_t socket, const char *path){
	//struct ubus_srv_ws *self = container_of(socket, struct ubus_srv_ws, api); 
	return -1; 
}

static void *_websocket_server_thread(void *ptr){
	struct ubus_srv_ws *self = (struct ubus_srv_ws*)ptr; 
	while(!self->shutdown){
		if(self->ctx) 
			lws_service(self->ctx, 100);	
	}
	return 0; 
}

static int _websocket_send(ubus_server_t socket, struct ubus_message **msg){
	struct ubus_srv_ws *self = container_of(socket, struct ubus_srv_ws, api); 
	pthread_mutex_lock(&self->qlock); 
	struct ubus_id *id = ubus_id_find(&self->clients, (*msg)->peer); 
	if(!id) {
		pthread_mutex_unlock(&self->qlock); 
		return -1; 
	}
	
	struct ubus_srv_ws_client *client = (struct ubus_srv_ws_client*)container_of(id, struct ubus_srv_ws_client, id);  
	struct ubus_srv_ws_frame *frame = ubus_srv_ws_frame_new(blob_head(&(*msg)->buf)); 
	list_add_tail(&frame->list, &client->tx_queue); 	
	pthread_mutex_unlock(&self->qlock); 
	ubus_message_delete(msg); 
	return 0; 
}

static void *_websocket_userdata(ubus_server_t socket, void *ptr){
	struct ubus_srv_ws *self = container_of(socket, struct ubus_srv_ws, api); 
	if(!ptr) return self->user_data; 
	self->user_data = ptr; 
	return ptr; 
}

static int _websocket_recv(ubus_server_t socket, struct ubus_message **msg){
	struct ubus_srv_ws *self = container_of(socket, struct ubus_srv_ws, api); 

	struct timespec t; 
	clock_gettime(CLOCK_REALTIME, &t); 
	// delay of 100ms
	t.tv_nsec+=100000000UL;

	pthread_mutex_lock(&self->qlock); 
	if(list_empty(&self->rx_queue)){
		if(pthread_cond_timedwait(&self->rx_ready, &self->qlock, &t) == ETIMEDOUT){
			pthread_mutex_unlock(&self->qlock); 
			return -EAGAIN; 
		}
	}
	if(list_empty(&self->rx_queue)) {
		pthread_mutex_unlock(&self->qlock); 
		return -EAGAIN; 
	}
	struct ubus_message *m = list_first_entry(&self->rx_queue, struct ubus_message, list); 
	list_del_init(&m->list); 
	*msg = m; 
	pthread_mutex_unlock(&self->qlock); 
	return 0; 
}


ubus_server_t ubus_srv_ws_new(const char *www_root){
	struct ubus_srv_ws *self = calloc(1, sizeof(struct ubus_srv_ws)); 
	self->www_root = (www_root)?www_root:"/www/"; 
	self->protocols = calloc(2, sizeof(struct lws_protocols)); 
	self->protocols[0] = (struct lws_protocols){
		.name = "",
		.callback = _ubus_socket_callback,
		.per_session_data_size = sizeof(struct ubus_srv_ws_client*),
		.user = self
	};
	ubus_id_tree_init(&self->clients); 
	pthread_mutex_init(&self->qlock, NULL); 
	pthread_cond_init(&self->rx_ready, NULL); 
	INIT_LIST_HEAD(&self->rx_queue); 
	static const struct ubus_server_api api = {
		.destroy = _websocket_destroy, 
		.listen = _websocket_listen, 
		.connect = _websocket_connect, 
		.send = _websocket_send, 
		.recv = _websocket_recv, 
		.userdata = _websocket_userdata
	}; 
	self->api = &api; 
	pthread_create(&self->thread, NULL, _websocket_server_thread, self); 
	return &self->api; 
}
