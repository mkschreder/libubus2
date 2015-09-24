#pragma once

struct ubus_object_type {
	char *name;
	uint32_t id;

	struct ubus_method *methods;
	int n_methods;
};

void ubus_object_type_init(struct ubus_object_type *self, const char *name); 
void ubus_object_type_destroy(struct ubus_object_type *self); 
