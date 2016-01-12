#include <stdio.h>
#include "../src/libubus2.h"
#include "../sockets/json_socket.h"
#include "../sockets/json_websocket.h"

bool running = true; 

void _on_message(ubus_socket_t socket, uint32_t peer, uint8_t type, uint32_t serial, struct blob_field *msg){ 
	printf("from socket %08x\n", peer); 
	if(msg) blob_field_dump_json(msg); 
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
	ubus_socket_t sock = json_socket_new(); 
	ubus_socket_on_message(sock, _on_message); 
	uint32_t id = 0; 
	if(0 != ubus_socket_connect(sock, "/var/run/ubus-json.sock", &id)){
		printf("could not connect to ubus!\n"); 
		return -1; 
	}

	printf("connected to ubus %08x\n", id); 

	struct ubus_context *ctx = ubus_new("ubus1", NULL); 

	signal(SIGINT, handle_sigint); 
	signal(SIGUSR1, do_crash_exit); 

	//ubus_connect(ctx, "localhost:1234", &id); 
	
	struct blob buf; 
	blob_init(&buf, 0, 0); 
	blob_put_string(&buf, "test"); 
	blob_put_string(&buf, "hello"); 
	blob_put_json(&buf, "{}"); 

	printf("sending frame: "); 
	blob_dump_json(&buf); 
	ubus_socket_send(sock, id, UBUS_MSG_METHOD_CALL, 1, blob_head(&buf)); 
	while(running){
		ubus_socket_handle_events(sock, 0); 
		ubus_handle_events(ctx); 
	}
	
	printf("cleaning up\n"); 
	ubus_delete(&ctx); 

	return 0; 
}
