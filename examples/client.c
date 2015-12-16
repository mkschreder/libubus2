#include <blobpack/blobpack.h>
#include <libusys/ustream.h>
#include <libutype/utils.h>

#include <libubus2.h>

static struct blob_buf bb;

static __attribute__((unused)) int test_method(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	void *t;

	blob_buf_init(&bb, 0, 0);

	t = blob_buf_open_table(&bb);
	blob_buf_put_string(&bb, "foo"); 
	blob_buf_put_string(&bb, 
	"1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
	"1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234569999"
	); 
	blob_buf_put_string(&bb, "bar"); 
	blob_buf_put_u32(&bb, 11);
	blob_buf_close_table(&bb, t);

	ubus_send_reply(ctx, req, blob_buf_head(&bb));
	return 0;
}
int main(int argc, char **argv)
{
	struct ubus_context *ctx = ubus_new(); 
	if(argc == 2) {
		printf("connecting to %s..\n", argv[1]); 
		ubus_connect(ctx, argv[1]); 
	} else {
		if(ubus_connect(ctx, NULL) < 0){
			fprintf(stderr, "%s: could not connect to ubus!\n", __FUNCTION__); 
			return -EIO;
		}
	}

	printf("connected as %08x\n", ctx->local_id);
	
static struct ubus_method test_object_methods[] = {
	UBUS_METHOD_NOARG("foo", test_method)
};

static struct ubus_object_type test_object_type =
	UBUS_OBJECT_TYPE("test-type", test_object_methods);
	static struct ubus_object test_object = {
		.name = "test",
		.type = &test_object_type,
		.methods = test_object_methods,
		.n_methods = ARRAY_SIZE(test_object_methods),
	};


	ubus_add_object(ctx, &test_object);
	
	printf("waiting for response!\n"); 
	while(true){
		ubus_handle_event(ctx); 
	}

	
	ubus_delete(&ctx);	

	return 0;
}

