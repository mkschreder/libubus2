#include <unistd.h>

#include <blobpack/blobpack.h>
#include <libusys/ustream.h>
#include <libutype/utils.h>

#include <libubus2.h>

#include <pthread.h>

static int done = 0; 

static int test_method(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_field *msg){
	void *t;

	//printf("TEST METHOD!\n"); 

	struct blob bb; 
	blob_init(&bb, 0, 0);

	t = blob_open_table(&bb);
	blob_put_string(&bb, "foo"); 
	blob_put_string(&bb, "data"); 
	blob_put_string(&bb, "bar"); 
	blob_put_int(&bb, 11);
	blob_close_table(&bb, t);

	ubus_request_resolve(req, blob_head(&bb)); 

	blob_free(&bb); 
	return 0;
}

static int _app_shutdown(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_field *msg){
	done = 1; 
	return 0; 
}

void _on_request_done(struct ubus_request *req, struct blob_field *res){
	printf("client-test: request succeeded! %s %s\n", req->object, req->method); 
	blob_field_dump_json(res); 
}

void _on_request_failed(struct ubus_request *req, struct blob_field *res){
	printf("client-test: request failed!\n"); 
	blob_field_dump_json(res); 
}

void *_server_thread(void *arg){
	struct ubus_server *server = ubus_server_new("ubus", NULL); 

	if(ubus_server_listen(server, "./ubus.sock") < 0){
		fprintf(stderr, "server could not listen on specified socket!\n"); 
		return NULL; 
	}

	while(!done){
		ubus_server_handle_events(server); 
	}

	ubus_server_delete(&server); 
	return NULL; 
}

void *_client_thread(void *arg){
	struct ubus_context *client = ubus_new("client", NULL, NULL); 

	if(ubus_connect(client, "./ubus.sock", NULL) < 0){
		fprintf(stderr, "%s: could not connect to ubus!\n", __FUNCTION__); 
		ubus_delete(&client); 
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

	struct blob buf; 
	blob_init(&buf, 0, 0); 
	blob_put_string(&buf, obj->name); 	
	ubus_object_serialize(obj, &buf); 

	//ubus_add_object(client, &obj); 

	// TODO: resolve the peer_id of "client" peer on server through user!
	// we can find out peer_id by looking at the objects on server peer (will be 0 if objects are native to server, otherwise will have ids as they are known to server peer)
	struct ubus_request *req = ubus_request_new("ubus", "/ubus/server", "ubus.server.publish", blob_head(&buf)); 
	ubus_request_on_resolve(req, &_on_request_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(client, &req); 

	obj = ubus_object_new("/app/shutdown"); 
	method = ubus_method_new("app.shutdown", _app_shutdown); 
	ubus_object_add_method(obj, &method); 

	blob_reset(&buf); 
	blob_put_string(&buf, obj->name); 	
	ubus_object_serialize(obj, &buf); 

	//ubus_add_object(client, &obj); 
	req = ubus_request_new("ubus", "/ubus/server", "ubus.server.publish", blob_head(&buf)); 
	ubus_request_on_resolve(req, &_on_request_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(client, &req); 

	blob_free(&buf); 

	while(!done){
		ubus_handle_events(client); 
	}

	blob_free(&buf); 
	ubus_delete(&client); 

	return NULL; 
}

int main(int argc, char **argv){
	pthread_t server, client; 
	pthread_create(&server, NULL, _server_thread, NULL); 

	usleep(2000000); 

	pthread_create(&client, NULL, _client_thread, NULL); 
	struct ubus_context *user = ubus_new("user", NULL, NULL); 

	struct blob buf; 
	blob_init(&buf, 0, 0); 

	signal(SIGPIPE, SIG_IGN); 

	if(ubus_connect(user, "./ubus.sock", NULL) < 0){
		fprintf(stderr, "%s: could not connect to ubus!\n", __FUNCTION__); 
		return -EIO;
	}

	sleep(1); 

	struct ubus_request *req = ubus_request_new("ubus", "/client/path/to/object", "my.object.test", blob_head(&buf)); 
	ubus_request_on_resolve(req, &_on_request_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(user, &req); 

	printf("waiting for response!\n"); 

	while(!done){
		ubus_handle_events(user); 
	}
	
	void *res; 
	pthread_join(client, &res); 
	pthread_join(server, &res); 

	blob_free(&buf); 
	ubus_delete(&user);	
	
	usleep(100000); 

	return 0;
}

