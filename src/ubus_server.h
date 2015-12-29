#pragma once

struct ubus_server {
	struct ubus_context *ctx; 
	struct list_head objects; 
}; 

struct ubus_server *ubus_server_new(const char *name); 
void ubus_server_delete(struct ubus_server **self); 

int ubus_server_listen(struct ubus_server *self, const char *path); 
int ubus_server_handle_events(struct ubus_server *self); 
