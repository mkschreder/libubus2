#pragma once 

typedef void (*ubus_subscriber_remove_handler_t)(struct ubus_context *ctx,
				      struct ubus_subscriber *obj, uint32_t id);

struct ubus_subscriber {
	struct ubus_object obj;
	struct ubus_method *watch_method; 
	ubus_request_handler_t cb;
	ubus_subscriber_remove_handler_t remove_cb;
};

typedef void (*ubus_state_handler_t)(struct ubus_context *ctx, struct ubus_object *obj);

