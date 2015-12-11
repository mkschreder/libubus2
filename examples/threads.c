#include "libubus2.h"

static int test_method(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	void *t;
	
	struct blob_buf bb; 
	blob_buf_init(&bb, 0, 0);

	t = blobmsg_open_table(&bb, "test");
	blobmsg_add_string(&bb, "foo", "bar"); 
	blobmsg_add_u32(&bb, "bar", 11);
	blobmsg_close_table(&bb, t);

	ubus_send_reply(ctx, req, bb.head);
	return 0;
}

static struct ubus_method test_object_methods[] = {
	UBUS_METHOD_NOARG("test", test_method)
};

static struct ubus_object_type test_object_type =
	UBUS_OBJECT_TYPE("test-type", test_object_methods);

static struct ubus_object test_object = {
	.name = "test",
	.type = &test_object_type,
	.methods = test_object_methods,
	.n_methods = ARRAY_SIZE(test_object_methods),
};

int main(int argc, char **argv){
	struct ubus_context *ctx = ubus_new(); 
	if(!ctx){
		fprintf(stderr, "no memory!\n"); 
		return -1; 
	}
	assert(ctx->buf.buf); 
	if(ubus_connect(ctx, "test.sock") < 0){
		printf("Error connecting to ubus socket!\n"); 
		return -1; 
	}

	assert(ctx->buf.buf); 
	ubus_add_object(ctx, &test_object); 

	while(true){
		ubus_handle_event(ctx); 
	}

	ubus_delete(&ctx); 
	return 0; 
}