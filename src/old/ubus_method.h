#pragma once 

#include "ubus_request.h"

struct ubus_method {
	char *name;
	ubus_request_handler_t handler;

	unsigned long mask;
	const struct blobmsg_policy *policy;
	int n_policy;
};

void ubus_method_init(struct ubus_method *self, const char *name, ubus_request_handler_t cb); 
void ubus_method_destroy(struct ubus_method *self); 
