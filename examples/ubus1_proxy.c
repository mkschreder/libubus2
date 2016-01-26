#include <stdio.h>
#include "../src/libubus2.h"

bool running = true; 

void _on_message(ubus_socket_t socket, uint32_t peer, struct blob_field *msg){ 
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
	ubus_socket_t sock = ubus_socket_new(); 
	uint32_t id = 0; 
	if(0 != ubus_socket_connect(sock, "json:///var/run/ubus-json.sock", &id)){
		printf("could not connect to ubus!\n"); 
		return -1; 
	}

	printf("connected to ubus %08x\n", id); 

	struct ubus_context *ctx = ubus_new("ubus1", NULL, NULL); 

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
	ubus_socket_send(sock, id, blob_head(&buf)); 
	while(running){
		ubus_socket_handle_events(sock, 0); 
		ubus_handle_events(ctx); 
	}
	
	printf("cleaning up\n"); 
	ubus_delete(&ctx); 

	return 0; 
}
