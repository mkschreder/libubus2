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

struct ubus_proxy *ubus_proxy_new(void){
	struct ubus_proxy *self = calloc(1, sizeof(struct ubus_proxy)); 
	ubus_id_tree_init(&self->clients_in); 
	ubus_id_tree_init(&self->clients_out); 
	return self; 
}

void ubus_proxy_delete(struct ubus_proxy **self){

}

static void _on_in_message_received(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg){
	struct ubus_proxy *self = (struct ubus_proxy*)ubus_socket_get_userdata(socket); 
	switch(type){
		case UBUS_MSG_PEER_CONNECTED: {
			// TODO: handle disconnects
			if(!self->outsock) break; 
			uint32_t outpeer = 0; 
			ubus_socket_connect(self->outsock, self->outpath, &outpeer); 	
			struct ubus_proxy_peer *p = ubus_proxy_peer_new(peer, outpeer); 	
			ubus_id_alloc(&self->clients_in, &p->id_in, peer); 
			ubus_id_alloc(&self->clients_out, &p->id_out, outpeer); 
			//printf("proxy: set up connection between %08x and %08x\n", peer, outpeer); 
			break; 
		}
		default: {
			struct ubus_id *id = ubus_id_find(&self->clients_in, peer); 
			if(!id) break; 
			struct ubus_proxy_peer *p = container_of(id, struct ubus_proxy_peer, id_in); 
			//printf("proxy: proxy request from %08x to %08x\n", peer, p->outpeer); 
			ubus_socket_send(self->outsock, p->outpeer, type, serial, msg); 
			break; 
		}
	}
}

static void _on_out_message_received(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg){
	struct ubus_proxy *self = (struct ubus_proxy*)ubus_socket_get_userdata(socket); 
	switch(type){
		default: {
			struct ubus_id *id = ubus_id_find(&self->clients_out, peer); 
			if(!id) break; 
			struct ubus_proxy_peer *p = container_of(id, struct ubus_proxy_peer, id_out); 
			//printf("proxy: proxy response from %08x to %08x\n", peer, p->inpeer); 
			ubus_socket_send(self->insock, p->inpeer, type, serial, msg); 
			break; 
		}

	}
}

int ubus_proxy_handle_events(struct ubus_proxy *self){
	ubus_socket_handle_events(self->insock, 0); 	
	ubus_socket_handle_events(self->outsock, 0); 	
	return 0; 
}

int ubus_proxy_listen(struct ubus_proxy *self, ubus_socket_t sock, const char *path){
	self->insock = sock; 
	ubus_socket_listen(sock, path); 
	ubus_socket_set_userdata(sock, self); 
	ubus_socket_on_message(sock, _on_in_message_received); 
	return 0; 
}

int ubus_proxy_connect(struct ubus_proxy *self, ubus_socket_t sock, const char *path){
	self->outsock = sock; 
	self->outpath = strdup(path); 
	ubus_socket_set_userdata(sock, self); 
	ubus_socket_on_message(sock, _on_out_message_received); 
	return 0; 
}

