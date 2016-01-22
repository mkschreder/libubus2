#pragma once

#include <inttypes.h>

struct ubus_client {
	struct ubus_context *ctx; 
}; 

struct ubus_client *ubus_client_new(const char *name); 
void ubus_client_delete(struct ubus_client **self); 

int ubus_client_connect(struct ubus_client *self, const char *path, uint32_t *con_id); 
int ubus_client_listen(struct ubus_client *self, const char *path); 
int ubus_client_handle_events(struct ubus_client *self); 

//static inline int ubus_client_add_object(struct ubus_client *self, struct ubus_object **obj){ return ubus_add_object(self->ctx, obj); }
