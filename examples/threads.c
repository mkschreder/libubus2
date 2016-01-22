#include "libubus2.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

static int test_method(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request *req, struct blob_field *msg)
{
	void *t;
	
	struct blob bb; 
	blob_init(&bb, 0, 0);

	t = blob_open_table(&bb);
	blob_put_string(&bb, "foo"); 
	blob_put_string(&bb, "bar"); 
	blob_put_string(&bb, "bar"); 
	blob_put_int(&bb, 11);
	blob_close_table(&bb, t);

	//ubus_send_reply(ctx, req, blob_head(&bb));
	return 0;
}

void* client_thread(void *args){
	struct ubus_context *ctx = ubus_new("client", NULL, NULL); 
		
	if(ubus_connect(ctx, "test.sock", NULL) < 0){
		printf("Error connecting to ubus socket!\n"); 
		return 0; 
	}

	struct ubus_object *obj = ubus_object_new("test"); 
	struct ubus_method *method = ubus_method_new("my.object.test", test_method); 
	ubus_method_add_param(method, "name_int", "i"); 
	ubus_method_add_param(method, "name_string", "ai"); 
	ubus_method_add_param(method, "name_table", "a{sv}"); 
	ubus_method_add_return(method, "some_return", "i"); 
	ubus_method_add_return(method, "some_table", "a{sv}"); // returns a dictionary
	
	ubus_object_add_method(obj, &method); 
	//ubus_add_object(ctx, &obj); 

	while(true){
		ubus_handle_events(ctx); 
	}

	ubus_delete(&ctx); 
}

int main(int argc, char **argv){
	pthread_t client1, client2; 
	pthread_create(&client1, NULL, client_thread, "client1"); 
	pthread_create(&client2, NULL, client_thread, "client2"); 
	while(true){
		sleep(1); 
	}
	return 0; 
}
