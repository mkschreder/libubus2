#include "libubus2.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

static int test_method(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	void *t;
	
	struct blob_buf bb; 
	blob_buf_init(&bb, 0, 0);

	t = blob_buf_open_table(&bb);
	blob_buf_put_string(&bb, "foo"); 
	blob_buf_put_string(&bb, "bar"); 
	blob_buf_put_string(&bb, "bar"); 
	blob_buf_put_u32(&bb, 11);
	blob_buf_close_table(&bb, t);

	ubus_send_reply(ctx, req, blob_buf_head(&bb));
	return 0;
}

static struct ubus_method test_object_methods[] = {
	UBUS_METHOD_NOARG("test", test_method)
};

static struct ubus_object_type test_object_type =
	UBUS_OBJECT_TYPE("test-type", test_object_methods);

void* client_thread(void *args){
	struct ubus_context *ctx = ubus_new(); 
	
	struct ubus_object test_object = {
		.name = args,
		.type = &test_object_type,
		.methods = test_object_methods,
		.n_methods = ARRAY_SIZE(test_object_methods),
	};

	if(!ctx){
		fprintf(stderr, "no memory!\n"); 
		return 0; 
	}
	assert(ctx->buf.buf); 
	if(ubus_connect(ctx, "test.sock") < 0){
		printf("Error connecting to ubus socket!\n"); 
		return 0; 
	}

	assert(ctx->buf.buf); 
	ubus_add_object(ctx, &test_object); 

	while(true){
		ubus_handle_event(ctx); 
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
