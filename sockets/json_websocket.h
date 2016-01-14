#pragma once 

#include <blobpack/blobpack.h>

struct ubus_socket_api; 
typedef const struct ubus_socket_api** ubus_socket_t; 

ubus_socket_t json_websocket_new(void); 

/*void json_websocket_delete(struct json_websocket **self); 

int json_websocket_listen(struct json_websocket *self, const char *path); 
int json_websocket_connect(struct json_websocket *self, const char *path); 

int json_websocket_handle_events(struct json_websocket *self); 

static inline void json_websocket_on_message(struct json_websocket *self, json_websocket_data_cb_t cb){ self->on_message = cb; } 
*/
