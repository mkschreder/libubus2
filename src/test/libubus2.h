#pragma once

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <libubox/blob.h>
#include <libubox/blobmsg.h>

struct ubus {
	void *socket; 
	void *context; 
	char *endpoint; 
}; 

struct ubus_object {

}; 

typedef enum {
	UBUS_ATTR_STATUS
} ubus_attr_t; 

struct ubus_request; 

typedef int (*ubus_request_handler_t)(struct ubus *ctx, struct ubus_object *obj, struct blob_buf *args);
typedef int (*ubus_response_handler_t)(struct ubus_request *req, struct blob_buf *res); 

void ubus_init(struct ubus *self); 
void ubus_destroy(struct ubus *self); 

int ubus_connect(struct ubus *self, const char *path); 
int ubus_disconnect(struct ubus *self); 

int ubus_publish_object(struct ubus *self, const char *path, struct ubus_object *obj); 
int ubus_call(struct ubus *self, const char *path, const char *method, struct blob_buf *args, ubus_response_handler_t cb); 

void ubus_object_init(struct ubus_object *self); 
void ubus_object_add_method(struct ubus_object *self, ubus_request_handler_t handler); 


struct ubus_request {
	struct ubus *ubus; 
	char *path; 
	char *method; 
	struct blob_buf *args; 
	struct blob_buf resp;
	ubus_response_handler_t cb; 
}; 

void ubus_request_init(struct ubus_request *req, const char *path, const char *method, struct blob_buf *args, ubus_response_handler_t cb); 

