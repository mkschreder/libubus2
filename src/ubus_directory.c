
#include "ubus_context.h"
#include "ubus_directory.h"

/*
static void _ubus_add_object_cb(struct ubus_request *req, int type, struct blob_attr *msg){
	struct ubus_object *obj = req->priv;

	if (!req->attrbuf[UBUS_ATTR_OBJID]) {
		return;
	}

	obj->id = blob_attr_get_u32(req->attrbuf[UBUS_ATTR_OBJID]);

	obj->avl.key = &obj->id;
	avl_insert(&req->ctx->objects, &obj->avl);
}
*/
int ubus_dir_publish_object(struct ubus_context *ctx, struct ubus_object **objptr){
	assert(ctx && objptr); 

	struct ubus_object *obj = *objptr; 
	struct list_head *pos; 
	
	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0);

	blob_buf_put_string(&buf, obj->name); 
	blob_offset_t ofs = blob_buf_open_array(&buf); 
	list_for_each(pos, &obj->methods){
		struct ubus_method *m = container_of(pos, struct ubus_method, list); 
		blob_buf_put_string(&buf, m->name); 
		blob_buf_put_attr(&buf, blob_buf_head(&m->signature)); 	
	}
	blob_buf_close_array(&buf, ofs); 

	*objptr = NULL; 

	int ret = 0; //ubus_call(ctx, "ubus", "/ubus/directory", "ubus.directory.publish", blob_buf_head(&buf), _ubus_add_object_cb, obj, 0); 
	//blob_buf_free(&buf); 
	
	if(ret < 0)
		return UBUS_STATUS_INVALID_ARGUMENT; 
/*
	if (ubus_start_request(ctx, &req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_ADD_OBJECT, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	req.raw_data_cb = ubus_add_object_cb;
	req.priv = obj;
	ret = ubus_complete_request(ctx, &req, 0);
	if (ret)
		return ret;

	if (!obj->id)
		return UBUS_STATUS_NO_DATA;
*/
	return 0;
}

static void _ubus_remove_object_cb(struct ubus_request *req, int type, struct blob_attr *msg){
	struct ubus_object *obj = req->priv;

	if (!req->attrbuf[UBUS_ATTR_OBJID])
		return;

	obj->id = 0;

	//if (req->attrbuf[UBUS_ATTR_OBJTYPE] && obj->type)
//		obj->type->id = 0;

	avl_delete(&req->ctx->objects, &obj->avl);
}

int ubus_remove_object(struct ubus_context *ctx, struct ubus_object *obj)
{

	blob_buf_reset(&ctx->buf);
	blob_buf_put_i32(&ctx->buf, obj->id);
	
	if(ubus_invoke(ctx, 0, "ubus.directory.publish", blob_buf_head(&ctx->buf), _ubus_remove_object_cb, NULL, 5000) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT; 
/*
	if (ubus_start_request(ctx, &req, blob_buf_head(&ctx->buf), blob_buf_size(&ctx->buf), UBUS_MSG_, 0) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	req.raw_data_cb = ubus_remove_object_cb;
	req.priv = obj;
	ret = ubus_complete_request(ctx, &req, 0);
	if (ret)
		return ret;

	if (obj->id)
		return UBUS_STATUS_NO_DATA;
*/
	return 0;
}
