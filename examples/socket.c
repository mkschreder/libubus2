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

#include <libubus2/libubus2.h>

void on_message1(struct ubus_socket *self, uint32_t peer, uint8_t type, uint32_t serial, struct blob_attr *msg){
	printf("message1: from %08x\n", peer); 
	blob_attr_dump_json(msg); 
}
void on_message2(struct ubus_socket *self, uint32_t peer, uint8_t type, uint32_t serial, struct blob_attr *msg){
	printf("message2\n"); 
}
void on_message3(struct ubus_socket *self, uint32_t peer, uint8_t type, uint32_t serial, struct blob_attr *msg){
	printf("message3\n"); 
}

int main(int argc, char **argv){
	struct ubus_socket *client1 = ubus_socket_new(); 
	struct ubus_socket *client2 = ubus_socket_new(); 
	struct ubus_socket *client3 = ubus_socket_new(); 

	ubus_socket_on_message(client1, &on_message1); 
	ubus_socket_on_message(client2, &on_message2); 
	ubus_socket_on_message(client3, &on_message3); 

	if(ubus_socket_listen(client1, "client1.sock") < 0){
		printf("client1: could not listen!\n"); 
	}
	
	printf("trying to connect..\n"); 

	if(ubus_socket_connect(client2, "client1.sock") < 0){
		printf("client2: could not connect!\n"); 
	}
	
	printf("client3 connecting..\n"); 
	if(ubus_socket_connect(client3, "client1.sock") < 0){
		printf("client3: could not connect!\n"); 
	}

	printf("processing events..\n"); 

	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 
	blob_buf_put_i32(&buf, 123); 

	ubus_socket_send(client2, UBUS_PEER_BROADCAST, 0, 0, blob_buf_head(&buf)); 

	while(true){
		//ubus_socket_send(client2, 0, 0, blob_buf_head(&buf)); 
		ubus_socket_poll(client1, 0); 
		ubus_socket_poll(client2, 0); 
		ubus_socket_poll(client3, 0); 
	}

	return 0; 
}
