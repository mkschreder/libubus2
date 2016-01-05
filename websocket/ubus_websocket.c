#include <libubus2/libubus2.h>
#include <blobpack/blobpack.h>

#include "ubus_websocket.h"
#include "../src/ubus_message.h"
#include <libutype/list.h>
#include <libwebsockets.h>
#include <assert.h>

struct lws_context; 
struct ubus_websocket {
	struct lws_context *ctx; 
	struct lws_protocols *protocols; 
	struct avl_tree clients; 
	struct blob buf; 
	ubus_socket_msg_cb_t on_message; 	
	const struct ubus_socket_api *api; 
	void *user_data; 
}; 

struct ubus_websocket_client {
	struct ubus_id id; 
	struct list_head tx_queue; 
}; 

struct ubus_websocket_frame {
	struct list_head list; 
	uint8_t *buf; 
	int len; 
	int sent_count; 
}; 

struct ubus_websocket_frame *ubus_websocket_frame_new(struct blob_field *msg){
	assert(msg); 
	struct ubus_websocket_frame *self = calloc(1, sizeof(struct ubus_websocket_frame)); 
	INIT_LIST_HEAD(&self->list); 
	char *json = blob_format_json(msg, false); 
	printf("frame: %s\n", json); 
	self->len = strlen(json); 
	self->buf = calloc(1, LWS_SEND_BUFFER_PRE_PADDING + self->len + LWS_SEND_BUFFER_POST_PADDING); 
	memcpy(self->buf + LWS_SEND_BUFFER_PRE_PADDING, json, self->len); 
	free(json); 
	self->sent_count = 0; 
	return self; 
}

void ubus_websocket_frame_delete(struct ubus_websocket_frame **self){
	free((*self)->buf); 
	free(*self); 
	*self = NULL; 
}


static struct ubus_websocket_client *ubus_websocket_client_new(void){
	struct ubus_websocket_client *self = calloc(1, sizeof(struct ubus_websocket_client)); 
	INIT_LIST_HEAD(&self->tx_queue); 
	return self; 
}

static __attribute__((unused)) void ubus_websocket_client_delete(struct ubus_websocket_client **self){
	// TODO: free tx_queue
	free(*self); 
	*self = NULL;
}

static void _parse_address(char *address, const char **host, const char **port){
	int addrlen = strlen(address); 
	for(int c = 0; c < addrlen; c++){
		if(address[c] == ':' && c != (addrlen - 1)) {
			address[c] = 0; 
			*host = address; 
			*port = address + c + 1; 
			break; 
		}
	}
}

static int _ubus_socket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *_user, void *in, size_t len){
	// TODO: keeping user data in protocol is probably not the right place. Fix it. 
	const struct lws_protocols *proto = lws_get_protocol(wsi); 

	struct ubus_websocket_client **user = (struct ubus_websocket_client **)_user; 

	int32_t peer_id = lws_get_socket_fd(wsi); 
	switch(reason){
		case LWS_CALLBACK_ESTABLISHED: {
			struct ubus_websocket *self = (struct ubus_websocket*)proto->user; 
			struct ubus_websocket_client *client = ubus_websocket_client_new(); 
			ubus_id_alloc(&self->clients, &client->id, 0); 
			*user = client; 
			char hostname[255], ipaddr[255]; 
			lws_get_peer_addresses(wsi, peer_id, hostname, sizeof(hostname), ipaddr, sizeof(ipaddr)); 
			printf("connection established! %s %s %d %08x\n", hostname, ipaddr, peer_id, client->id.id); 
			if(self->on_message) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_PEER_CONNECTED, 0, NULL); 
			break; 
		}
		case LWS_CALLBACK_CLOSED: {
			printf("client disconnected\n"); 
			ubus_websocket_client_delete(user); 
			break; 
		}
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			if(list_empty(&(*user)->tx_queue)){
				break; 
			}
			// TODO: handle partial writes correctly 
			struct ubus_websocket_frame *frame = list_first_entry(&(*user)->tx_queue, struct ubus_websocket_frame, list);
			int n = lws_write(wsi, &frame->buf[LWS_SEND_BUFFER_PRE_PADDING], frame->len, LWS_WRITE_TEXT);// | LWS_WRITE_NO_FIN);
			if(n < 0) return -1; 
			//printf("wrote %d bytes of %d\n", n, frame->len); 
			frame->sent_count += n; 
			if(frame->sent_count >= frame->len){
				list_del_init(&frame->list); 
				ubus_websocket_frame_delete(&frame); 
			}
			lws_rx_flow_control(wsi, 1); 
			break; 
		}
		case LWS_CALLBACK_RECEIVE: {
			assert(proto); 
			assert(user); 
			if(!user) break; 
			struct ubus_websocket *self = (struct ubus_websocket*)proto->user; 
			//printf("client data %s %p\n", (char*)in, self->buf.buf); 
			blob_reset(&self->buf); 
			if(blob_put_json_from_string(&self->buf, in)){
				struct blob_field *rpcobj = blob_field_first_child(blob_head(&self->buf)); 
				if(!blob_field_validate(rpcobj, "sssisssa")) break; 
				if(self->on_message){
					// parse out jsonrpc 
					struct blob_field *k, *v, *params = NULL, *result = NULL, *error = NULL; 
					const char *method = NULL;  
					uint32_t id = 0; 
					blob_field_for_each_kv(rpcobj, k, v){
						if(strcmp(blob_field_get_string(k), "jsonrpc") == 0 && strcmp(blob_field_get_string(v), "2.0") != 0) return 0;   
						else if(strcmp(blob_field_get_string(k), "method") == 0) method = blob_field_get_string(v);  
						else if(strcmp(blob_field_get_string(k), "id") == 0) id = blob_field_get_int(v);  
						else if(strcmp(blob_field_get_string(k), "params") == 0) params = v;  
						else if(strcmp(blob_field_get_string(k), "result") == 0) result = v;  
						else if(strcmp(blob_field_get_string(k), "error") == 0) error = v;  
					}
					if(id && method && strcmp(method, "call") == 0 && params) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_METHOD_CALL, id, params); 
					else if(method && params) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_SIGNAL, 0, params); 
					else if(id && result) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_METHOD_RETURN, id, result); 
					else if(id && error) self->on_message(&self->api, (*user)->id.id, UBUS_MSG_ERROR, id, error); 
				}
			}
			lws_rx_flow_control(wsi, 0); 
			lws_callback_on_writable(wsi); 	
			break; 
		}
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("client error\n"); 
			break; 
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			printf("Client connected!\n"); 
			break;
		default: 
			break; 
	}; 

	return 0; 
}


void _websocket_destroy(ubus_socket_t socket){
	struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	blob_free(&self->buf); 
	if(self->ctx) lws_context_destroy(self->ctx); 
	free(self->protocols); 
	free(self);  
}

int _websocket_listen(ubus_socket_t socket, const char *path){
	struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	struct lws_context_creation_info info; 
	memset(&info, 0, sizeof(info)); 

	int addrlen = strlen(path); 
	char *address = alloca(addrlen);
	strcpy(address, path); 
	const char *host, *port; 
	_parse_address(address, &host, &port); 

	info.port = atoi(port);
	info.gid = -1; 
	info.uid = -1; 
	info.user = self; 
	info.protocols = self->protocols; 
	info.extensions = lws_get_internal_extensions();
	info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;

	self->ctx = lws_create_context(&info); 

	return 0; 
}

int _websocket_connect(ubus_socket_t socket, const char *path, uint32_t *id){
	//struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	return 0; 
}

static int _websocket_handle_events(ubus_socket_t socket, int timeout){
	struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	return lws_service(self->ctx, timeout); 	
}

static int _websocket_send(ubus_socket_t socket, int32_t peer, int type, uint16_t serial, struct blob_field *msg){
	struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	struct ubus_id *id = ubus_id_find(&self->clients, peer); 
	if(!id) return -1; 
	
	struct ubus_websocket_client *client = (struct ubus_websocket_client*)container_of(id, struct ubus_websocket_client, id);  
	printf("websocket send: "); 
	blob_field_dump_json(msg); 
	blob_reset(&self->buf); 
	blob_offset_t ofs = blob_open_table(&self->buf); 
	blob_put_string(&self->buf, "jsonrpc"); 
	blob_put_string(&self->buf, "2.0"); 
	blob_put_string(&self->buf, "id"); 
	blob_put_int(&self->buf, serial); 
	// TODO: add support for all message types
	switch(type){
		case UBUS_MSG_METHOD_RETURN: {
			blob_put_string(&self->buf, "result"); 
			blob_put_attr(&self->buf, msg); 
			break; 
		}
	}
	blob_close_table(&self->buf, ofs); 

	struct ubus_websocket_frame *frame = ubus_websocket_frame_new(blob_head(&self->buf)); 
	list_add(&frame->list, &client->tx_queue); 

	return 0; 
}

static void _websocket_on_message(ubus_socket_t socket, ubus_socket_msg_cb_t cb){
	struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	self->on_message = cb; 
}

static void *_websocket_userdata(ubus_socket_t socket, void *ptr){
	struct ubus_websocket *self = container_of(socket, struct ubus_websocket, api); 
	if(!ptr) return self->user_data; 
	self->user_data = ptr; 
	return ptr; 
}

ubus_socket_t ubus_websocket_new(void){
	struct ubus_websocket *self = calloc(1, sizeof(struct ubus_websocket)); 
	blob_init(&self->buf, 0, 0); 
	self->protocols = calloc(2, sizeof(struct lws_protocols)); 
	self->protocols[0] = (struct lws_protocols){
		.name = "",
		.callback = _ubus_socket_callback,
		.per_session_data_size = sizeof(struct ubus_websocket_client*),
		.user = self
	};
	ubus_id_tree_init(&self->clients); 
	static const struct ubus_socket_api api = {
		.destroy = _websocket_destroy, 
		.listen = _websocket_listen, 
		.connect = _websocket_connect, 
		.send = _websocket_send, 
		.handle_events = _websocket_handle_events, 
		.on_message = _websocket_on_message, 
		.userdata = _websocket_userdata
	}; 
	self->api = &api; 
	return &self->api; 
}
