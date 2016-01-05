#pragma once 

#include <blobpack/blobpack.h>

ubus_socket_t ubus_websocket_new(void); 

/*void ubus_websocket_delete(struct ubus_websocket **self); 

int ubus_websocket_listen(struct ubus_websocket *self, const char *path); 
int ubus_websocket_connect(struct ubus_websocket *self, const char *path); 

int ubus_websocket_handle_events(struct ubus_websocket *self); 

static inline void ubus_websocket_on_message(struct ubus_websocket *self, ubus_websocket_data_cb_t cb){ self->on_message = cb; } 
*/
