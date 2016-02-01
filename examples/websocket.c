#include <stdio.h>
#include "../src/libubus2.h"
#include "../src/ubus_srv_ws.h"

bool running = true; 

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
	signal(SIGINT, handle_sigint); 
	signal(SIGUSR1, do_crash_exit); 

	ubus_server_t server = ubus_srv_ws_new(NULL); 

	if(ubus_server_listen(server, "ws://localhost:1234") < 0){
		fprintf(stderr, "server could not listen on specified socket!\n"); 
		return 0; 
	}

	while(running){
		struct ubus_message *msg = NULL; 
		if(ubus_server_recv(server, &msg) < 0){
			continue; 
		}
		printf("got message from %08x: ", msg->peer); 
		blob_dump_json(&msg->buf); 
		if(ubus_server_send(server, &msg) < 0){
			printf("could not send echo reply!\n"); 
		}
	}
	
	printf("cleaning up\n"); 
	ubus_server_delete(server); 

	return 0; 
}
