#include <stdio.h>
#include "../src/libubus2.h"
#include "../sockets/json_websocket.h"
#include "../sockets/json_socket.h"

bool running = true; 

void _on_message(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg){ 
	printf("got %s\n", ubus_message_types[type]); 
	blob_field_dump_json(msg); 
}

void handle_sigint(){
	printf("Interrupted!\n"); 
	running = false; 
}

void do_crash_exit(){
	printf("Debug exit!\n"); 
	int *ptr = 0; 
	*ptr = 0xdeadbeef; 
}

int main(int argc, char **argv){
	ubus_socket_t insock = json_websocket_new(); 
	ubus_socket_t outsock = json_socket_new(); 
	struct ubus_proxy *proxy = ubus_proxy_new(&insock, &outsock); 
	ubus_proxy_listen(proxy, "localhost:1234"); 
	ubus_proxy_connect(proxy, "localhost:1235"); 

	signal(SIGINT, handle_sigint); 
	signal(SIGUSR1, do_crash_exit); 

	ubus_socket_t ssock = json_socket_new(); 
	struct ubus_server *server = ubus_server_new("ubus", &ssock); 

	if(ubus_server_listen(server, "localhost:1235") < 0){
		fprintf(stderr, "server could not listen on specified socket!\n"); 
		return 0; 
	}

	while(running){
		ubus_proxy_handle_events(proxy); 
		ubus_server_handle_events(server); 
	}
	
	printf("cleaning up\n"); 
	//ubus_server_delete(&server); 
	ubus_proxy_delete(&proxy); 

	return 0; 
}
