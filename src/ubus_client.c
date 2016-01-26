#include "ubus_context.h"
#include "ubus_client.h"

static int _on_ubus_client_list(struct ubus_method *method, struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request *req, struct blob_field *msg){
	struct blob buf; 
	blob_init(&buf, 0, 0); 

	/*struct ubus_object *o = NULL; 
	avl_for_each_element(&ctx->objects_by_name, o, avl){
		blob_put_string(&buf, o->name); 
		ubus_object_serialize(o, &buf); 
	}
*/
	ubus_request_resolve(req, blob_head(&buf)); 	
	blob_free(&buf); 

	return 0; 
}

struct ubus_client *ubus_client_new(const char *name){
	struct ubus_client *self = calloc(1, sizeof(struct ubus_client)); 

	struct ubus_object *obj = ubus_object_new("ubus"); 
	struct ubus_method *method = ubus_method_new("list", _on_ubus_client_list); 
	ubus_object_add_method(obj, &method); 
	ubus_object_set_userdata(obj, self); 

	self->ctx = ubus_new(name, &obj); 

	ubus_set_userdata(self->ctx, self); 

	return self; 
}

void ubus_client_delete(struct ubus_client **self){
	assert(self); 
	ubus_delete(&(*self)->ctx); 
	free(*self); 
	*self = NULL; 
}

int ubus_client_connect(struct ubus_client *self, const char *path, uint32_t *con_id){
	return ubus_connect(self->ctx, path, con_id); 
}

int ubus_client_listen(struct ubus_client *self, const char *path){
	return ubus_listen(self->ctx, path); 
}

int ubus_client_handle_events(struct ubus_client *self){
	return ubus_handle_events(self->ctx); 
}
