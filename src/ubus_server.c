#include "ubus_context.h"
#include "ubus_server.h"
#include <blobpack/blobpack.h>

void _on_forward_response(struct ubus_request *req, struct blob_attr *res){
	//printf("forwarded request succeeded!\n"); 
	struct ubus_request *or = (struct ubus_request*)ubus_request_get_userdata(req); 
	ubus_request_resolve(or, res); 
}

void _on_request_failed(struct ubus_request *req, struct blob_attr *res){
	//printf("ERROR request failed!\n"); 
}

static int _on_forward_call(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_attr *msg){
	char **forward_info = (char**)obj->priv; 
	//printf("forward call from %s to %s %s\n", req->dst_name, forward_info[0], forward_info[1]); 
	struct ubus_request *r = ubus_request_new(forward_info[0], forward_info[1], self->name, msg); 
	ubus_request_on_resolve(r, &_on_forward_response); 
	ubus_request_on_reject(r, &_on_request_failed); 
	ubus_request_set_userdata(r, req); 
	ubus_send_request(ctx, &r); 

	return 0; 
}

static int _on_list_objects(struct ubus_method *m, struct ubus_context *self, struct ubus_object *_obj, struct ubus_request *req, struct blob_attr *msg){
	//printf("list objects\n"); 	
	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 

	struct ubus_object *obj = NULL; 
	avl_for_each_element(&self->objects, obj, avl){
		//printf("got object %s\n", obj->name); 
		blob_buf_put_string(&buf, obj->name); 
		ubus_object_serialize(obj, &buf); 
	}

	ubus_request_resolve(req, blob_buf_head(&buf)); 	
	blob_buf_free(&buf); 

	return 0; 
}

static int _on_publish_object(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *_obj, struct ubus_request *req, struct blob_attr *msg){
	//printf("server: on_publish_object from %s!\n", req->dst_name); 	
	char path[255]; 
	struct blob_attr *params[3]; 

	msg = blob_attr_first_child(msg); 
	if(!blob_attr_parse(msg, "s[sa]", params, 2)) return UBUS_STATUS_INVALID_ARGUMENT; 
	const char *objname = blob_attr_get_string(params[0]); 	

	snprintf(path, sizeof(path), "/%s%s", req->dst_name, objname); 

	// create proxy object for the published object
	struct ubus_object *obj = ubus_object_new(path); 
	struct blob_attr *mname, *margs; 
	blob_attr_for_each_kv(params[1], mname, margs){
		struct ubus_method *method = ubus_method_new(blob_attr_get_string(mname), _on_forward_call); 
		struct blob_attr *arg; 
		// copy over each argument 
		blob_attr_for_each_child(margs, arg){ 
			blob_buf_put_attr(&method->signature, arg); 
		}
		ubus_object_add_method(obj, &method); 
	}

	char **forward_info = calloc(2, sizeof(char*)); 
	forward_info[0] = strdup(req->dst_name); 
	forward_info[1] = strdup(objname); 
	ubus_object_set_userdata(obj, forward_info); 
	ubus_publish_object(ctx, &obj); 

	//printf("object published!\n"); 

	return 0; 
}

struct ubus_server *ubus_server_new(const char *name){
	struct ubus_server *self = calloc(1, sizeof(struct ubus_server)); 
	
	self->ctx = ubus_new(name); 

	struct ubus_object *obj = ubus_object_new("/ubus/server"); 
	struct ubus_method *method = ubus_method_new("ubus.server.publish", _on_publish_object); 
	ubus_method_add_param(method, "name", "s"); 
	ubus_method_add_param(method, "signature", "[sa]"); 
	ubus_object_add_method(obj, &method); 

	method = ubus_method_new("ubus.server.list", _on_list_objects); 
	ubus_object_add_method(obj, &method); 
	ubus_object_set_userdata(obj, self); 

	ubus_publish_object(self->ctx, &obj); 
	return self; 
}

int ubus_server_listen(struct ubus_server *self, const char *path){
	return ubus_listen(self->ctx, path); 
}

int ubus_server_handle_events(struct ubus_server *self){
	return ubus_handle_events(self->ctx); 
}
