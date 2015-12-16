#pragma once

#include <libutype/avl.h>

struct ubus_context; 

struct ubus_object {
	struct avl_node avl;

	char *name;
	uint32_t id;

	char *path;
	struct ubus_object_type *type;

	void (*subscribe_cb)(struct ubus_context *ctx, struct ubus_object *obj);
	bool has_subscribers;

	struct ubus_method *methods;
	int n_methods;
};

struct ubus_object_data {
	uint32_t id;
	uint32_t type_id;
	uint32_t client_id; 
	const char *path;
	struct blob_attr *signature;
};

void ubus_object_init(struct ubus_object *obj); 
void ubus_object_destroy(struct ubus_object *obj);

typedef void (*ubus_lookup_handler_t)(struct ubus_context *ctx,
				      struct ubus_object_data *obj,
				      void *priv);


