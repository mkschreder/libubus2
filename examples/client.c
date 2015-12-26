#include <unistd.h>

#include <blobpack/blobpack.h>
#include <libusys/ustream.h>
#include <libutype/utils.h>

#include <libubus2.h>

#include <pthread.h>

static int test_method(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_attr *msg){
	void *t;

	printf("TEST METHOD!\n"); 

	struct blob_buf bb; 
	blob_buf_init(&bb, 0, 0);

	t = blob_buf_open_table(&bb);
	blob_buf_put_string(&bb, "foo"); 
	blob_buf_put_string(&bb, "data"); 
	blob_buf_put_string(&bb, "bar"); 
	blob_buf_put_u32(&bb, 11);
	blob_buf_close_table(&bb, t);

	ubus_request_resolve(req, blob_buf_head(&bb)); 

	blob_buf_free(&bb); 
	return 0;
}

void _on_list_reply(struct ubus_request *req, struct blob_attr *res){
	printf("list reply!\n"); 
	blob_attr_dump_json(res); 
}

void _on_request_done(struct ubus_request *req, struct blob_attr *res){
	printf("request succeeded! %s\n", req->object); 
	blob_attr_dump_json(res); 
}

void _on_request_failed(struct ubus_request *req, struct blob_attr *res){
	printf("request failed!\n"); 
}

void *_server_thread(void *arg){
	struct ubus_server *server = ubus_server_new("ubus"); 

	if(ubus_server_listen(server, "127.0.0.1:1234") < 0){
		fprintf(stderr, "server could not listen on specified socket!\n"); 
		return NULL; 
	}

	while(true){
		ubus_server_handle_events(server); 
	}

	return NULL; 
}

void *_client_thread(void *arg){
	struct ubus_context *client = ubus_new("client"); 

	if(ubus_connect(client, "127.0.0.1:1234") < 0){
		fprintf(stderr, "%s: could not connect to ubus!\n", __FUNCTION__); 
		return NULL;
	}
	
	struct ubus_object *obj = ubus_object_new("/path/to/object"); 
	struct ubus_method *method = ubus_method_new("my.object.test", test_method); 
	ubus_method_add_param(method, "name_int", "i"); 
	ubus_method_add_param(method, "name_string", "ai"); 
	ubus_method_add_param(method, "name_table", "a{sv}"); 
	ubus_method_add_return(method, "some_return", "i"); 
	ubus_method_add_return(method, "some_table", "a{sv}"); // returns a dictionary
 	
	ubus_object_add_method(obj, &method); 

	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 
	blob_buf_put_string(&buf, obj->name); 	
	ubus_object_serialize(obj, &buf); 

	ubus_publish_object(client, &obj); 

		// TODO: resolve the peer_id of "client" peer on server through user!
	// we can find out peer_id by looking at the objects on server peer (will be 0 if objects are native to server, otherwise will have ids as they are known to server peer)
	struct ubus_request *req = ubus_request_new("ubus", "/ubus/server", "ubus.server.publish", blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_request_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(client, &req); 

	blob_buf_free(&buf); 

	while(true){
		ubus_handle_events(client); 
	}
	
	ubus_delete(&client); 

	return NULL; 
}

int main(int argc, char **argv){
	pthread_t server, client; 
	pthread_create(&server, NULL, _server_thread, NULL); 
	pthread_create(&client, NULL, _client_thread, NULL); 

	usleep(200); 

	struct ubus_context *user = ubus_new("user"); 

	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 

	signal(SIGPIPE, SIG_IGN); 

	if(ubus_connect(user, "127.0.0.1:1234") < 0){
		fprintf(stderr, "%s: could not connect to ubus!\n", __FUNCTION__); 
		return -EIO;
	}

	sleep(1); 

	struct ubus_request *req = ubus_request_new("ubus", "/client/path/to/object", "my.object.test", blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_request_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(user, &req); 

	//printf("connected as %08x\n", ctx->local_id);
	req = ubus_request_new("ubus", "/ubus/server", "ubus.server.list", blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_list_reply); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(user, &req); 

	req = ubus_request_new("ubus", "/client/test/object", "foo", blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_list_reply); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(user, &req); 

	printf("waiting for response!\n"); 

	while(true){
		ubus_handle_events(user); 
	}
	
	ubus_delete(&user);	

	return 0;
}

