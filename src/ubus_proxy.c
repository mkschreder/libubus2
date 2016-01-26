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

#include <stdio.h>
#include <stdlib.h>
#include "ubus_proxy.h"
#include "ubus_message.h"

struct ubus_proxy_peer {
	struct ubus_id id_in; 
	struct ubus_id id_out; 
	uint32_t inpeer; 
	uint32_t outpeer; 
}; 

struct ubus_proxy_peer *ubus_proxy_peer_new(uint32_t inpeer, uint32_t outpeer){
	struct ubus_proxy_peer *self = calloc(1, sizeof(struct ubus_proxy_peer)); 
	self->inpeer = inpeer; 
	self->outpeer = outpeer; 
	return self; 
}

void ubus_proxy_peer_delete(struct ubus_proxy_peer **self){
	free(*self); 
	*self = NULL; 
}

struct ubus_proxy *ubus_proxy_new(ubus_socket_t *insock, ubus_socket_t *outsock){
	struct ubus_proxy *self = calloc(1, sizeof(struct ubus_proxy)); 
	ubus_id_tree_init(&self->clients_in); 
	ubus_id_tree_init(&self->clients_out); 
	self->insock = *insock; *insock = NULL; 
	self->outsock = *outsock; *outsock = NULL; 
	return self; 
}

void ubus_proxy_delete(struct ubus_proxy **self){
	if((*self)->outpath) free((*self)->outpath); 
	ubus_socket_delete((*self)->insock); 
	ubus_socket_delete((*self)->outsock); 
	struct ubus_id *id = 0, *ptr; 
	avl_for_each_element_safe(&(*self)->clients_in, id, avl, ptr){
		struct ubus_proxy_peer *peer = container_of(id, struct ubus_proxy_peer, id_in); 
		ubus_proxy_peer_delete(&peer); 
	}
	free(*self); 
	*self = NULL; 
}

static void _on_in_message_received(ubus_socket_t socket, uint32_t peer, struct blob_field *msg){
	struct ubus_proxy *self = (struct ubus_proxy*)ubus_socket_get_userdata(socket); 
	struct ubus_id *id = ubus_id_find(&self->clients_in, peer); 
	struct ubus_proxy_peer *p = NULL; 
	if(id){
		p = container_of(id, struct ubus_proxy_peer, id_in); 
	} else {
		if(!self->outsock) return; 
		uint32_t outpeer = 0; 
		if(ubus_socket_connect(self->outsock, self->outpath, &outpeer) < 0){
			printf("proxy: could not connect to outgoing socket! %s\n", self->outpath);
			return; 
		}
		p = ubus_proxy_peer_new(peer, outpeer); 	
		ubus_id_alloc(&self->clients_in, &p->id_in, peer); 
		ubus_id_alloc(&self->clients_out, &p->id_out, outpeer); 
		printf("proxy: peer connected %08x!\n", p->id_in.id); 
		//printf("proxy: set up connection between %08x and %08x\n", peer, outpeer); 

	} 
	printf("proxy: proxy request from %08x to %08x\n", peer, p->outpeer); 
	printf("proxy request message: "); blob_field_dump_json(msg); 
	if(ubus_socket_send(self->outsock, p->outpeer, msg) < 0){
		printf("proxy message send failed to outsocket\n"); 
	}
	
	return; 
	//TODO: fix client connect/disconnect
	/*switch(type){
		case UBUS_MSG_PEER_CONNECTED: {
			// TODO: handle disconnects
			if(!self->outsock) break; 
			uint32_t outpeer = 0; 
			ubus_socket_connect(self->outsock, self->outpath, &outpeer); 	
			struct ubus_proxy_peer *p = ubus_proxy_peer_new(peer, outpeer); 	
			ubus_id_alloc(&self->clients_in, &p->id_in, peer); 
			ubus_id_alloc(&self->clients_out, &p->id_out, outpeer); 
			printf("proxy: peer connected %08x!\n", p->id_in.id); 
			//printf("proxy: set up connection between %08x and %08x\n", peer, outpeer); 
			break; 
		}
		case UBUS_MSG_PEER_DISCONNECTED: {
			// remove the client binding and close connection to the main server as well
			struct ubus_id *id = ubus_id_find(&self->clients_in, peer); 
			if(id){
				printf("proxy: peer disconnect %08x!\n", id->id); 
				struct ubus_proxy_peer *p = container_of(id, struct ubus_proxy_peer, id_in); 
				ubus_socket_disconnect(self->outsock, p->id_out.id); 
				ubus_id_free(&self->clients_out, &p->id_out); 
				ubus_id_free(&self->clients_in, &p->id_in); 
				ubus_proxy_peer_delete(&p); 
			}
			break; 
		}
		default: {
			struct ubus_id *id = ubus_id_find(&self->clients_in, peer); 
			if(!id) break; 
			struct ubus_proxy_peer *p = container_of(id, struct ubus_proxy_peer, id_in); 
			printf("proxy: proxy request from %08x to %08x\n", peer, p->outpeer); 
			ubus_socket_send(self->outsock, p->outpeer, msg); 
			break; 
		}
	}*/
}

static void _on_out_message_received(ubus_socket_t socket, uint32_t peer, struct blob_field *msg){
	struct ubus_proxy *self = (struct ubus_proxy*)ubus_socket_get_userdata(socket); 
	struct ubus_id *id = ubus_id_find(&self->clients_out, peer); 
	if(!id) return; 
	struct ubus_proxy_peer *p = container_of(id, struct ubus_proxy_peer, id_out); 
	//printf("proxy: proxy response from %08x to %08x\n", peer, p->inpeer); 
	printf("proxy return message: "); blob_field_dump_json(msg); 
	if(ubus_socket_send(self->insock, p->inpeer, msg) < 0){
		ubus_socket_disconnect(self->outsock, p->id_out.id); 
		ubus_id_free(&self->clients_out, &p->id_out); 
		ubus_id_free(&self->clients_in, &p->id_in); 
		ubus_proxy_peer_delete(&p); 

		printf("proxy peer disconnected!\n"); 
	}
}

int ubus_proxy_handle_events(struct ubus_proxy *self){
	ubus_socket_handle_events(self->insock, 0); 	
	ubus_socket_handle_events(self->outsock, 0); 	
	return 0; 
}

int ubus_proxy_listen(struct ubus_proxy *self, const char *path){
	ubus_socket_listen(self->insock, path); 
	ubus_socket_set_userdata(self->insock, self); 
	ubus_socket_on_message(self->insock, _on_in_message_received); 
	return 0; 
}

int ubus_proxy_connect(struct ubus_proxy *self, const char *path){
	self->outpath = strdup(path); 
	ubus_socket_set_userdata(self->outsock, self); 
	ubus_socket_on_message(self->outsock, _on_out_message_received); 
	return 0; 
}

