#pragma once

#include <libusys/uloop.h>
#include <libutype/avl.h>
#include <blobpack/blobpack.h>

#define UBUS_UNIX_SOCKET "/var/run/ubus.sock"
#define UBUS_MAX_MSGLEN 10240000
#define UBUS_MAX_NOTIFY_PEERS	16

#include "ubus_message.h"
#include "ubus_request.h"
#include "ubus_method.h"
#include "ubus_object_type.h"
#include "ubus_object.h"
#include "ubus_socket.h"
#include "ubus_context.h"
#include "ubus_subscriber.h"


struct ubus_event_handler; 
struct ubus_notify_request; 
struct ubus_request; 

typedef void (*ubus_connect_handler_t)(struct ubus_context *ctx);
typedef void (*ubus_event_handler_t)(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg);
struct ubus_event_handler {
	struct ubus_object obj;

	ubus_event_handler_t cb;
};

typedef void (*ubus_notify_complete_handler_t)(struct ubus_notify_request *req, int idx, int ret);
struct ubus_notify_request {
	struct ubus_request req;

	ubus_notify_complete_handler_t status_cb;
	ubus_notify_complete_handler_t complete_cb;

	uint32_t pending;
	uint32_t id[UBUS_MAX_NOTIFY_PEERS + 1];
};

struct ubus_pending_data {
	struct list_head list;
	int type;
	struct blob_attr data[];
};


struct ubus_context {
	struct list_head requests;
	struct avl_tree objects;
	struct list_head pending;

	struct uloop_fd sock;
	struct uloop_timeout pending_timer;

	uint32_t local_id;
	uint16_t request_seq;
	int stack_depth;

	void (*connection_lost)(struct ubus_context *ctx);

	struct ubus_msghdr_buf msgbuf;
	uint32_t msgbuf_data_len;
	int msgbuf_reduction_counter;

	struct blob_buf buf; 
	struct uloop *uloop; 
};

struct ubus_auto_conn {
	struct ubus_context ctx;
	struct uloop_timeout timer;
	const char *path;
	ubus_connect_handler_t cb;
};


struct ubus_context *ubus_new(void); 
void ubus_delete(struct ubus_context **self); 

void ubus_init(struct ubus_context *self); 
void ubus_destroy(struct ubus_context *ctx);

int ubus_connect(struct ubus_context *self, const char *path);
void ubus_handle_event(struct ubus_context *ctx); 
void ubus_set_auto_connect(struct ubus_context *self, struct ubus_auto_conn *conn);
int ubus_reconnect(struct ubus_context *self, const char *path);

const char *ubus_strerror(int error);

/* ----------- raw request handling ----------- */

/* wait for a request to complete and return its status */
int ubus_complete_request(struct ubus_context *ctx, struct ubus_request *req,
			  int timeout);

/* complete a request asynchronously */
void ubus_complete_request_async(struct ubus_context *ctx,
				 struct ubus_request *req);

/* abort an asynchronous request */
void ubus_abort_request(struct ubus_context *ctx, struct ubus_request *req);

/* ----------- objects ----------- */

int ubus_lookup(struct ubus_context *ctx, const char *path,
		ubus_lookup_handler_t cb, void *priv);

int ubus_lookup_id(struct ubus_context *ctx, const char *path, uint32_t *id);

/* make an object visible to remote connections */
int ubus_add_object(struct ubus_context *ctx, struct ubus_object *obj);

/* remove the object from the ubus connection */
int ubus_remove_object(struct ubus_context *ctx, struct ubus_object *obj);

/* add a subscriber notifications from another object */
int ubus_register_subscriber(struct ubus_context *ctx, struct ubus_subscriber *obj);
int ubus_unregister_subscriber(struct ubus_context *ctx, struct ubus_subscriber *obj); 

int ubus_subscribe(struct ubus_context *ctx, struct ubus_subscriber *obj, uint32_t id);
int ubus_unsubscribe(struct ubus_context *ctx, struct ubus_subscriber *obj, uint32_t id);

/* ----------- rpc ----------- */

/* invoke a method on a specific object */
int ubus_invoke(struct ubus_context *ctx, uint32_t obj, const char *method,
		struct blob_attr *msg, ubus_data_handler_t cb, void *priv,
		int timeout);

/* asynchronous version of ubus_invoke() */
int ubus_invoke_async(struct ubus_context *ctx, uint32_t obj, const char *method,
		      struct blob_attr *msg, struct ubus_request *req);

/* send a reply to an incoming object method call */
int ubus_send_reply(struct ubus_context *ctx, struct ubus_request_data *req,
		    struct blob_attr *msg);


static inline void ubus_defer_request(struct ubus_context *ctx,
				      struct ubus_request_data *req,
				      struct ubus_request_data *new_req)
{
    memcpy(new_req, req, sizeof(*req));
    req->deferred = true;
}

static inline void ubus_request_set_fd(struct ubus_context *ctx,
				       struct ubus_request_data *req, int fd)
{
    req->fd = fd;
}

void ubus_complete_deferred_request(struct ubus_context *ctx,
				    struct ubus_request_data *req, int ret);

/*
 * send a notification to all subscribers of an object
 * if timeout < 0, no reply is expected from subscribers
 */
int ubus_notify(struct ubus_context *ctx, struct ubus_object *obj,
		const char *type, struct blob_attr *msg, int timeout);

int ubus_notify_async(struct ubus_context *ctx, struct ubus_object *obj,
		      const char *type, struct blob_attr *msg,
		      struct ubus_notify_request *req);


/* ----------- events ----------- */

int ubus_send_event(struct ubus_context *ctx, const char *id,
		    struct blob_attr *data);

int ubus_register_event_handler(struct ubus_context *ctx,
				struct ubus_event_handler *ev,
				const char *pattern);

static inline int ubus_unregister_event_handler(struct ubus_context *ctx,
						struct ubus_event_handler *ev){
    return ubus_remove_object(ctx, &ev->obj);
}

