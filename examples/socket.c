/*
 * Copyright (C) 2015 Martin Schröder <mkschreder.uk@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../src/libubus2.h"

void on_message1(ubus_socket_t socket, uint32_t peer, struct blob_field *msg){
	printf("message1: from %08x\n", peer); 
	blob_field_dump_json(msg); 
}
void on_message2(ubus_socket_t socket, uint32_t peer, struct blob_field *msg){
	printf("message2\n"); 
}
void on_message3(ubus_socket_t socket, uint32_t peer, struct blob_field *msg){
	printf("message3\n"); 
}

int main(int argc, char **argv){
	ubus_socket_t client1 = ubus_rawsocket_new(); 
	ubus_socket_t client2 = ubus_rawsocket_new(); 
	ubus_socket_t client3 = ubus_rawsocket_new(); 

	ubus_socket_on_message(client1, &on_message1); 
	ubus_socket_on_message(client2, &on_message2); 
	ubus_socket_on_message(client3, &on_message3); 

	if(ubus_socket_listen(client1, "client1.sock") < 0){
		printf("client1: could not listen!\n"); 
	}
	
	printf("trying to connect..\n"); 

	if(ubus_socket_connect(client2, "client1.sock", NULL) < 0){
		printf("client2: could not connect!\n"); 
	}
	
	printf("client3 connecting..\n"); 
	if(ubus_socket_connect(client3, "client1.sock", NULL) < 0){
		printf("client3: could not connect!\n"); 
	}

	printf("processing events..\n"); 

	struct blob buf; 
	blob_init(&buf, 0, 0); 
	blob_put_int(&buf, 123); 

	ubus_socket_send(client2, UBUS_PEER_BROADCAST, blob_head(&buf)); 

	while(true){
		//ubus_socket_send(client2, 0, 0, blob_head(&buf)); 
		ubus_socket_handle_events(client1, 0); 
		ubus_socket_handle_events(client2, 0); 
		ubus_socket_handle_events(client3, 0); 
	}

	return 0; 
}
