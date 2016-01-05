#pragma once

#include "ubus_id.h"
#include "ubus_socket.h"

struct ubus_proxy {
	struct avl_tree clients_in; 
	struct avl_tree clients_out; 
	ubus_socket_t insock; 
	ubus_socket_t outsock; 
	char *outpath; 
}; 

struct ubus_proxy *ubus_proxy_new(void); 
void ubus_proxy_delete(struct ubus_proxy **self); 

int ubus_proxy_handle_events(struct ubus_proxy *self); 

int ubus_proxy_listen(struct ubus_proxy *self, ubus_socket_t sock, const char *path); 
int ubus_proxy_connect(struct ubus_proxy *self, ubus_socket_t sock, const char *path); 

