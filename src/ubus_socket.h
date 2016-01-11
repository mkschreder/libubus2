#pragma once

#include <inttypes.h>

#define UBUS_PEER_BROADCAST (-1)

struct ubus_socket_api; 
typedef const struct ubus_socket_api** ubus_socket_t; 
struct blob_field; 

typedef void (*ubus_socket_msg_cb_t)(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg);  
//typedef void (*ubus_socket_client_cb_t)(struct ubus_rawsocket *self, uint32_t peer);  

struct ubus_socket_api {
	void 	(*poll)(ubus_socket_t ptr, int timeout); 
	void 	(*destroy)(ubus_socket_t ptr); 
	int 	(*listen)(ubus_socket_t ptr, const char *path); 
	int 	(*connect)(ubus_socket_t ptr, const char *path, uint32_t *id);
	int 	(*disconnect)(ubus_socket_t ptr, uint32_t id); 
	int 	(*send)(ubus_socket_t ptr, int32_t peer, int type, uint16_t serial, struct blob_field *msg); 
	int 	(*handle_events)(ubus_socket_t ptr, int timeout); 
	void 	(*on_message)(ubus_socket_t ptr, ubus_socket_msg_cb_t cb); 		
	void*	(*userdata)(ubus_socket_t ptr, void *data); 
}; 

#define UBUS_TARGET_PEER (0)
#define UBUS_BROADCAST_PEER (-1)

#define ubus_socket_delete(sock) {(*sock)->destroy(sock); sock = NULL;} 
#define ubus_socket_listen(sock, path) (*sock)->listen(sock, path)
#define ubus_socket_connect(sock, path, clidptr) (*sock)->connect(sock, path, clidptr) 
#define ubus_socket_disconnect(sock, clid) (*sock)->disconnect(sock, clid) 
#define ubus_socket_send(sock, peer, type, serial, msg) (*sock)->send(sock, peer, type, serial, msg)
#define ubus_socket_on_message(sock, cb) (*sock)->on_message(sock, cb)
#define ubus_socket_handle_events(sock, timeout) (*sock)->handle_events(sock, timeout)
#define ubus_socket_sockopt(sock, opt, ptr) (*sock)->sockopt(sock, opt, ptr)
#define ubus_socket_get_userdata(sock) (*sock)->userdata(sock, NULL)
#define ubus_socket_set_userdata(sock, ptr) (*sock)->userdata(sock, ptr)
