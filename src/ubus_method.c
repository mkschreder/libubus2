#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include "ubus_method.h"

#include <string.h>

void ubus_method_init(struct ubus_method *self, const char *name, ubus_request_handler_t cb){
	if(name) self->name = strdup(name); 
	else self->name = 0; 
	self->handler = cb; 
}

void ubus_method_destroy(struct ubus_method *self){
	if(self->name) free(self->name); 
	self->handler = 0; 
}

