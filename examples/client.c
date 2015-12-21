#include <blobpack/blobpack.h>
#include <libusys/ustream.h>
#include <libutype/utils.h>

#include <libubus2.h>

static __attribute__((unused)) int test_method(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request *req, const char *method,
		  struct blob_attr *msg){
	void *t;

	struct blob_buf bb; 
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

	//ubus_send_reply(ctx, req, blob_buf_head(&bb));
	blob_buf_free(&bb); 
	return 0;
}

void _on_request_done(struct ubus_request *req, struct blob_attr *res){
	printf("request succeeded!\n"); 
	blob_attr_dump_json(res); 
}

void _on_request_failed(struct ubus_request *req, int code, struct blob_attr *res){
	printf("request failed!\n"); 
}

int main(int argc, char **argv){
	struct ubus_context *client = ubus_new("client"); 
	struct ubus_context *server = ubus_new("server"); 

	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 

	if(ubus_listen(server, "ubus.sock") < 0){
		fprintf(stderr, "could not start listening on specified socket!\n"); 
		return -EIO; 
	}

	if(ubus_connect(client, "ubus.sock") < 0){
		fprintf(stderr, "%s: could not connect to ubus!\n", __FUNCTION__); 
		return -EIO;
	}

	//printf("connected as %08x\n", ctx->local_id);
	
	struct ubus_object *obj = ubus_object_new("/path/to/object"); 
	struct ubus_method *method = ubus_method_new("my.object.test", test_method); 
	ubus_method_add_param(method, "name_int", "i"); 
	ubus_method_add_param(method, "name_string", "ai"); 
	ubus_method_add_param(method, "name_table", "a{sv}"); 
	ubus_method_add_return(method, "some_return", "i"); 
	ubus_method_add_return(method, "some_table", "a{sv}"); // returns a dictionary
 	
	ubus_object_add_method(obj, &method); 

	printf("publishing object\n"); 

	ubus_publish_object(client, &obj); 

	blob_buf_reset(&buf); 
	blob_buf_put_string(&buf, "argument"); 

	struct ubus_request *req = ubus_request_new("client", "/path/to/object", "my.object.test", blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_request_done); 
	ubus_request_on_fail(req, &_on_request_failed); 
	ubus_send_request(server, &req); 

	printf("waiting for response!\n"); 
	while(true){
		ubus_handle_events(client); 
		ubus_handle_events(server); 
	}
	
	ubus_delete(&server); 
	ubus_delete(&client);	

	return 0;
}

