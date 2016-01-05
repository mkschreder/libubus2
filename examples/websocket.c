#include <stdio.h>
#include "../src/libubus2.h"
#include "../websocket/ubus_websocket.h"

void _on_message(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg){ 
	printf("got %s\n", ubus_message_types[type]); 
	blob_field_dump_json(msg); 
}

int main(int argc, char **argv){
	ubus_socket_t insock = ubus_websocket_new(); 
	ubus_socket_t outsock = ubus_rawsocket_new(); 
	struct ubus_proxy *proxy = ubus_proxy_new(); 
	ubus_proxy_listen(proxy, insock, "localhost:1234"); 
	ubus_proxy_connect(proxy, outsock, "localhost:1235"); 

	struct ubus_server *server = ubus_server_new("ubus", NULL); 

	if(ubus_server_listen(server, "localhost:1235") < 0){
		fprintf(stderr, "server could not listen on specified socket!\n"); 
		return 0; 
	}

	while(true){
		ubus_proxy_handle_events(proxy); 
		ubus_server_handle_events(server); 
	}

	ubus_server_delete(&server); 

	return 0; 
}
