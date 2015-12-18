#pragma once

#include <inttypes.h>
#include <blobpack/blobpack.h>

#include "ubus_object.h"
#include "ubus_message.h"

struct ubus_request_data {
	uint32_t object;
	uint32_t peer;
	uint16_t seq;

	/* internal use */
	uint8_t deferred;
	int fd;
};

struct ubus_request; 

typedef void (*ubus_data_handler_t)(struct ubus_request *req,
				    int type, struct blob_attr *msg);
typedef void (*ubus_fd_handler_t)(struct ubus_request *req, int fd);
typedef void (*ubus_complete_handler_t)(struct ubus_request *req, int ret);

struct ubus_request {
	struct list_head list;

	struct list_head pending;
	int status_code;
	bool status_msg;
	bool blocked;
	bool cancelled;
	bool notify;

	struct blob_attr *attrbuf[UBUS_ATTR_MAX]; 

	uint32_t peer;
	uint16_t seq;

	ubus_data_handler_t raw_data_cb;
	ubus_data_handler_t data_cb;
	ubus_fd_handler_t fd_cb;
	ubus_complete_handler_t complete_cb;

	struct ubus_context *ctx;
	void *priv;
};

struct ubus_request *ubus_request_new(void); 
void ubus_request_delete(struct ubus_request **self); 

void ubus_request_set_status(struct ubus_request *req, int ret); 

