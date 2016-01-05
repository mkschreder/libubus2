#include "ubus_context.h"
#include "ubus_server.h"
#include <blobpack/blobpack.h>

struct ubus_forward_info {
	struct list_head list; 
	uint32_t attached_id; 
	char *client; 
	char *object; 
}; 

struct ubus_forward_info *forward_info_new(void){
	struct ubus_forward_info *self = calloc(1, sizeof(struct ubus_forward_info)); 
	INIT_LIST_HEAD(&self->list); 
	return self; 
}

void _on_forward_response(struct ubus_request *req, struct blob_field *res){
	//printf("forwarded request succeeded!\n"); 
	struct ubus_request *or = (struct ubus_request*)ubus_request_get_userdata(req); 
	ubus_request_resolve(or, res); 
}

void _on_forward_failed(struct ubus_request *req, struct blob_field *res){
	//printf("ERROR request failed!\n"); 
	struct ubus_request *or = (struct ubus_request*)ubus_request_get_userdata(req); 
	ubus_request_reject(or, res); 
}

static int _on_forward_call(struct ubus_method *self, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_field *msg){
	struct ubus_forward_info *info = ubus_object_get_userdata(obj); 
	assert(info); 

	//printf("forward call from %s to %s %s\n", req->dst_name, forward_info[0], forward_info[1]); 
	struct ubus_request *r = ubus_request_new(info->client, info->object, self->name, msg); 
	ubus_request_on_resolve(r, &_on_forward_response); 
	ubus_request_on_reject(r, &_on_forward_failed); 
	ubus_request_set_userdata(r, req); 
	ubus_send_request(ctx, &r); 

	return 0; 
}

static int _on_list_objects(struct ubus_method *m, struct ubus_context *self, struct ubus_object *_obj, struct ubus_request *req, struct blob_field *msg){
	//printf("list objects\n"); 	
	struct blob buf; 
	blob_init(&buf, 0, 0); 

	struct ubus_object *obj = NULL; 
	avl_for_each_element(&self->objects_by_name, obj, avl){
		//printf("got object %s\n", obj->name); 
		blob_put_string(&buf, obj->name); 
		ubus_object_serialize(obj, &buf); 
	}

	ubus_request_resolve(req, blob_head(&buf)); 	
	blob_free(&buf); 

	return 0; 
}

static int _on_publish_object(struct ubus_method *m, struct ubus_context *ctx, struct ubus_object *_obj, struct ubus_request *req, struct blob_field *msg){
	assert(msg); 

	struct ubus_server *self = ubus_get_userdata(ctx); 
	//printf("server: on_publish_object from %s!\n", req->dst_name); 	
	char path[255]; 
	struct blob_field *params[3]; 

	msg = blob_field_first_child(msg); 
	if(!blob_field_parse(msg, "s[sa]", params, 2)) return UBUS_STATUS_INVALID_ARGUMENT; 
	const char *objname = blob_field_get_string(params[0]); 	

	//blob_field_dump_json(msg); 
	snprintf(path, sizeof(path), "/%s%s", req->dst_name, objname); 

	// create proxy object for the published object
	struct ubus_object *obj = ubus_object_new(path); 
	struct blob_field *mname, *margs; 
	blob_field_for_each_kv(params[1], mname, margs){
		struct ubus_method *method = ubus_method_new(blob_field_get_string(mname), _on_forward_call); 
		struct blob_field *arg; 
		// copy over each argument 
		blob_field_for_each_child(margs, arg){ 
			blob_put_attr(&method->signature, arg); 
		}
		ubus_object_add_method(obj, &method); 
	}

	struct ubus_forward_info *info = forward_info_new(); 
	info->client = strdup(req->dst_name); 
	info->object = strdup(objname); 
	ubus_object_set_userdata(obj, info); 

	info->attached_id = ubus_add_object(ctx, &obj); 

	list_add(&info->list, &self->objects); 

	ubus_request_resolve(req, NULL); 
	//printf("object published!\n"); 

	return 0; 
}

struct ubus_server *ubus_server_new(const char *name, ubus_socket_t *socket){
	struct ubus_server *self = calloc(1, sizeof(struct ubus_server)); 
	
	self->ctx = ubus_new(name, socket); 
	INIT_LIST_HEAD(&self->objects); 

	struct ubus_object *obj = ubus_object_new("/ubus/server"); 
	struct ubus_method *method = ubus_method_new("ubus.server.publish", _on_publish_object); 
	ubus_method_add_param(method, "name", "s"); 
	ubus_method_add_param(method, "signature", "[sa]"); 
	ubus_object_add_method(obj, &method); 

	method = ubus_method_new("ubus.server.list", _on_list_objects); 
	ubus_object_add_method(obj, &method); 
	ubus_object_set_userdata(obj, self); 

	ubus_add_object(self->ctx, &obj); 

	ubus_set_userdata(self->ctx, self); 
	return self; 
}

void ubus_server_delete(struct ubus_server **self){
	struct ubus_forward_info *info, *tmp; 
	// free the info objects but we can only do this like this because we also free objects
	// otherwise we would have to look up each object as well and set userdata to null!
	list_for_each_entry_safe(info, tmp, &(*self)->objects, list){
		free(info->client); 
		free(info->object); 
		free(info); 
	}
	ubus_delete(&(*self)->ctx); 
	free(*self); 
	*self = NULL; 
}

int ubus_server_listen(struct ubus_server *self, const char *path){
	return ubus_listen(self->ctx, path); 
}

int ubus_server_handle_events(struct ubus_server *self){
	return ubus_handle_events(self->ctx); 
}
