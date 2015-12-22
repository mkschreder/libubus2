/*
 * Copyright (C) 2015 Martin Schröder <mkschreder.uk@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#ifdef FreeBSD
#include <sys/param.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <libusys/usock.h>

#include <blobpack/blobpack.h>

#include "ubus_socket.h"

#define STATIC_IOV(_var) { .iov_base = (char *) &(_var), .iov_len = sizeof(_var) }

#define UBUS_MSGBUF_REDUCTION_INTERVAL	16

struct ubus_msg_header {
	uint8_t hdr_size; 	// works as a magic. Must always be sizeof(struct ubus_msg_header)
	uint8_t type;  		// type of request 
	uint16_t seq;		// request sequence that is set by sender 
	uint32_t peer;		// destination peer  
	uint32_t data_size;	// length of the data that follows 
} __attribute__((packed)) __attribute__((__aligned__(4))); 

struct ubus_frame {
	struct list_head list; 
	struct ubus_msg_header hdr; 
	struct blob_buf data; 

	int send_count; 
}; 

struct ubus_client {
	struct ubus_id id; 
	struct list_head tx_queue; 
	int fd; 

	int recv_count; 
	struct ubus_msg_header hdr; 
	struct blob_buf data; 
	//struct list_head rx_queue; 
}; 

struct ubus_client *ubus_client_new(int fd){
	struct ubus_client *self = calloc(1, sizeof(struct ubus_client)); 
	INIT_LIST_HEAD(&self->tx_queue); 
	self->fd = fd; 
	self->recv_count = 0; 
	blob_buf_init(&self->data, 0, 0); 
	return self; 
}

struct ubus_frame *ubus_frame_new(uint32_t peer, int type, uint16_t seq, struct blob_attr *msg){
	struct ubus_frame *self = calloc(1, sizeof(struct ubus_frame)); 
	blob_buf_init(&self->data, (char*)msg, blob_attr_pad_len(msg)); 
	INIT_LIST_HEAD(&self->list); 
	self->hdr.peer = peer; 
	self->hdr.type = type;  
	self->hdr.seq = seq; 
	self->hdr.data_size = blob_attr_pad_len(msg); 
	return self; 
}

void ubus_frame_delete(struct ubus_frame **self){
	blob_buf_free(&(*self)->data); 
	free(*self); 
	*self = NULL; 
}

struct ubus_socket *ubus_socket_new(void){
	struct ubus_socket *self = calloc(1, sizeof(struct ubus_socket)); 
	ubus_socket_init(self); 
	return self; 
}

void ubus_socket_delete(struct ubus_socket **self){
	free(*self); 
	*self = NULL; 
}

void ubus_socket_init(struct ubus_socket *self){
	//INIT_LIST_HEAD(&self->clients); 
	ubus_id_tree_init(&self->clients); 
}

void ubus_socket_destroy(struct ubus_socket *self){
	// TODO
}

static void _accept_connection(struct ubus_socket *self){
	bool done = false; 

	do {
		int client = accept(self->listen_fd, NULL, 0);
		if ( client < 0 ) {
			switch (errno) {
			case ECONNABORTED:
			case EINTR:
				done = true;
			default:
				return;  
			}
		}

		// configure client into non blocking mode
		fcntl(client, F_SETFL, fcntl(client, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

		struct ubus_client *cl = ubus_client_new(client); 
		ubus_id_alloc(&self->clients, &cl->id, 0); 
		
		if(self->on_client_connected){
			self->on_client_connected(self, cl->id.id); 
		}
		
		//printf("new client: %08x\n", cl->id.id); 
	} while (!done);
}


int ubus_socket_listen(struct ubus_socket *self, const char *unix_socket){
	unlink(unix_socket);
	umask(0177);
	self->listen_fd = usock(USOCK_UNIX | USOCK_SERVER | USOCK_NONBLOCK, unix_socket, NULL);
	if (self->listen_fd < 0) {
		perror("usock");
		return -1; 
	}
	return 0; 
}

int ubus_socket_connect(struct ubus_socket *self, const char *path){
	int fd = usock(USOCK_UNIX, path, NULL);
	if (fd < 0)
		return -UBUS_STATUS_CONNECTION_FAILED;

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

	struct ubus_client *cl = ubus_client_new(fd); 
	ubus_id_alloc(&self->clients, &cl->id, 0); 
	
	// connecting out generates the same event as connecting in
	if(self->on_client_connected){
		self->on_client_connected(self, cl->id.id); 
	}

	return 0; 
}

void _ubus_client_recv(struct ubus_client *self, struct ubus_socket *socket){
	// if we still have not received the header
	if(self->recv_count < sizeof(struct ubus_msg_header)){
		int rc = recv(self->fd, ((char*)&self->hdr) + self->recv_count, sizeof(struct ubus_msg_header) - self->recv_count, 0); 
		if(rc > 0){
			self->recv_count += rc; 
		} 
	}
	// if we have just received the header then we allocate the body based on size in the header
	if(self->recv_count == sizeof(struct ubus_msg_header)){
		// TODO: validate header here!
		//printf("got header! %d bytes, data of %d bytes header size should be %d\n", self->recv_count, self->hdr.data_size, (int)sizeof(struct ubus_msg_header)); 
		blob_buf_resize(&self->data, self->hdr.data_size); 
	}
	// if we have received the header then we receive the body here
	if(self->recv_count >= sizeof(struct ubus_msg_header)){
		int rc = 0; 
		int cursor = self->recv_count - sizeof(struct ubus_msg_header); 
		while((rc = recv(self->fd, (char*)(blob_buf_head(&self->data)) + cursor, self->hdr.data_size - cursor, 0)) > 0){
			self->recv_count += rc; 
			cursor = self->recv_count - sizeof(struct ubus_msg_header);
		}
		// if we have received the full message then we call the message callback
		if(self->recv_count == (sizeof(struct ubus_msg_header) + self->hdr.data_size)){
			//printf("full message received %d bytes\n", self->hdr.data_size); 
			if(socket->on_message){
				socket->on_message(socket, self->hdr.peer, self->id.id, self->hdr.type, self->hdr.seq, blob_buf_head(&self->data)); 
			}
			self->recv_count = 0; 
		}
	}
}

void _ubus_client_send(struct ubus_client *self){
	if(list_empty(&self->tx_queue)){
		return; 
	}

	struct ubus_frame *req = list_first_entry(&self->tx_queue, struct ubus_frame, list);
	if(req->send_count < sizeof(struct ubus_msg_header)){
		int sc = send(self->fd, ((char*)&req->hdr) + req->send_count, sizeof(struct ubus_msg_header) - req->send_count, 0); 
		if(sc > 0){
			req->send_count += sc; 
		}
	}
	if(req->send_count >= sizeof(struct ubus_msg_header)){
		int cursor = req->send_count - sizeof(struct ubus_msg_header); 
		int sc; 
		int buf_size = blob_attr_pad_len(blob_buf_head(&req->data)); 
		while((sc = send(self->fd, blob_buf_head(&req->data) + cursor, buf_size - cursor, 0)) > 0){
			req->send_count += sc; 
			cursor = req->send_count - sizeof(struct ubus_msg_header);
		}
		if(req->send_count == (sizeof(struct ubus_msg_header) + buf_size)){
			// full buffer was transmitted so we destroy the request
			list_del_init(&req->list); 
			//printf("removed completed request from queue! %d bytes\n", req->send_count); 
			ubus_frame_delete(&req); 
		}
	}
}

void ubus_socket_poll(struct ubus_socket *self, int timeout){
	int count = avl_size(&self->clients) + 1; 
	struct pollfd *pfd = alloca(sizeof(struct pollfd) * count); 
	memset(pfd, 0, sizeof(struct pollfd) * count); 
	struct ubus_client **clients = alloca(sizeof(void*)); 
	pfd[0] = (struct pollfd){ .fd = self->listen_fd, .events = POLLIN | POLLERR }; 
	clients[0] = 0; 

	int c = 1; 
	struct ubus_id *id;  
	avl_for_each_element(&self->clients, id, avl){
		struct ubus_client *client = (struct ubus_client*)container_of(id, struct ubus_client, id);  
		pfd[c] = (struct pollfd){ .fd = client->fd, .events = POLLIN | POLLERR };  
		clients[c] = client;  
		c++; 
	}		
	
	// try to send more data
	for(int c = 1; c < count; c++){
		_ubus_client_send(clients[c]); 
	}

	int ret = 0; 
	if((ret = poll(pfd, count, timeout)) > 0){
		if(pfd[0].revents > 0){
			_accept_connection(self);
		}
		for(int c = 1; c < count; c++){
			if(pfd[c].revents > 0){
				//printf("fd %d %d has data!\n", c, clients[c]->fd); 
				// receive as much data as we can
				_ubus_client_recv(clients[c], self); 
			}
		}
	}
}

int ubus_socket_send(struct ubus_socket *self, int32_t peer, uint32_t target, int type, uint16_t serial, struct blob_attr *msg){
	struct ubus_id *id;  
	if(peer == UBUS_PEER_BROADCAST){
		avl_for_each_element(&self->clients, id, avl){
			struct ubus_client *client = (struct ubus_client*)container_of(id, struct ubus_client, id);  
			struct ubus_frame *req = ubus_frame_new(target, type, serial, msg);
			list_add(&req->list, &client->tx_queue); 
			//printf("added request to tx_queue!\n"); 
		}		
	} else {
		struct ubus_id *id = ubus_id_find(&self->clients, peer); 
		if(!id) return -1; 
		struct ubus_client *client = (struct ubus_client*)container_of(id, struct ubus_client, id);  
		struct ubus_frame *req = ubus_frame_new(target, type, serial, msg);
		list_add(&req->list, &client->tx_queue); 
	}
	return 0; 	
}

#if 0
static void wait_data(int fd, bool write)
{
	struct pollfd pfd = { .fd = fd };

	pfd.events = write ? POLLOUT : POLLIN;
	poll(&pfd, 1, 0);
}

static int writev_retry(int fd, struct iovec *iov, int iov_len, int sock_fd)
{
	static struct {
		struct cmsghdr h;
		int fd;
	} fd_buf = {
		.h = {
			.cmsg_len = sizeof(fd_buf),
			.cmsg_level = SOL_SOCKET,
			.cmsg_type = SCM_RIGHTS,
		}
	};
	struct msghdr msghdr = {
		.msg_iov = iov,
		.msg_iovlen = iov_len,
		.msg_control = &fd_buf,
		.msg_controllen = sizeof(fd_buf),
	};
	int len = 0;

	do {
		int cur_len;

		if (sock_fd < 0) {
			msghdr.msg_control = NULL;
			msghdr.msg_controllen = 0;
		} else {
			fd_buf.fd = sock_fd;
		}

		cur_len = sendmsg(fd, &msghdr, 0);
		if (cur_len < 0) {
			switch(errno) {
			case EAGAIN:
				wait_data(fd, true);
				break;
			case EINTR:
				break;
			default:
				return -1;
			}
			continue;
		}

		if (len > 0)
			sock_fd = -1;

		len += cur_len;
		while (cur_len >= iov->iov_len) {
			cur_len -= iov->iov_len;
			iov_len--;
			iov++;
			if (!iov_len)
				return len;
		}
		iov->iov_base += cur_len;
		iov->iov_len -= cur_len;
		msghdr.msg_iov = iov;
		msghdr.msg_iovlen = iov_len;
	} while (1);

	/* Should never reach here */
	return -1;
}

int ubus_socket_send_raw(struct ubus_socket *self, uint32_t seq,
			   void *msg, size_t size, int cmd, uint32_t peer, int fd)
{
	struct ubus_msghdr hdr;
	struct iovec iov[2] = {
		STATIC_IOV(hdr)
	};
	int ret;

	hdr.version = 0;
	hdr.type = cmd;
	hdr.seq = seq;
	hdr.peer = peer;

	if (!msg) {
		blob_buf_reset(&self->buf);
		msg = blob_buf_head(&self->buf);
	}

	iov[1].iov_base = (char *) msg;
	iov[1].iov_len = size; ;

	ret = writev_retry(self->sock.fd, iov, ARRAY_SIZE(iov), fd);
	if (ret < 0)
		self->sock.eof = true;

	if (fd >= 0)
		close(fd);

	return ret;
}

int ubus_socket_send(struct ubus_socket *self, uint32_t peer, int type, struct blob_attr *msg){
	return ubus_socket_send_raw(self, 0, msg, blob_attr_pad_len(msg), type, peer, 0);  
}

static int recv_retry(int fd, struct iovec *iov, bool wait, int *recv_fd){
	int bytes, total = 0;
	static struct {
		struct cmsghdr h;
		int fd;
	} fd_buf = {
		.h = {
			.cmsg_type = SCM_RIGHTS,
			.cmsg_level = SOL_SOCKET,
			.cmsg_len = sizeof(fd_buf),
		},
	};
	struct msghdr msghdr = {
		.msg_iov = iov,
		.msg_iovlen = 1,
	};

	while (iov->iov_len > 0) {
		if (wait)
			wait_data(fd, false);

		if (recv_fd) {
			msghdr.msg_control = &fd_buf;
			msghdr.msg_controllen = sizeof(fd_buf);
		} else {
			msghdr.msg_control = NULL;
			msghdr.msg_controllen = 0;
		}

		fd_buf.fd = -1;
		bytes = recvmsg(fd, &msghdr, 0);
		if (!bytes)
			return -1;

		if (bytes < 0) {
			bytes = 0;
			//if (self->cancelled)
			//	return 0;
			if (errno == EINTR)
				continue;

			if (errno != EAGAIN)
				return -1;
		}
		if (!wait && !bytes)
			return 0;

		if (recv_fd)
			*recv_fd = fd_buf.fd;

		recv_fd = NULL;

		wait = true;
		iov->iov_len -= bytes;
		iov->iov_base += bytes;
		total += bytes;
	}

	return total;
}

static bool ubus_validate_hdr(struct ubus_msghdr *hdr)
{
	struct blob_attr *data = (struct blob_attr *) (hdr + 1);

	if (hdr->version != 0)
		return false;

	if (blob_attr_raw_len(data) < sizeof(*data))
		return false;

	if (blob_attr_pad_len(data) > UBUS_MAX_MSGLEN)
		return false;

	return true;
}

static bool alloc_msg_buf(struct ubus_socket *self, int len)
{
	void *ptr;
	int buf_len = self->msgbuf_data_len;
	int rem;

	if (!self->msgbuf.data)
		buf_len = 0;

	rem = (len % UBUS_MSG_CHUNK_SIZE);
	if (rem > 0)
		len += UBUS_MSG_CHUNK_SIZE - rem;

	if (len < buf_len &&
	    ++self->msgbuf_reduction_counter > UBUS_MSGBUF_REDUCTION_INTERVAL) {
		self->msgbuf_reduction_counter = 0;
		buf_len = 0;
	}

	if (len <= buf_len)
		return true;

	ptr = realloc(self->msgbuf.data, len);
	if (!ptr)
		return false;

	self->msgbuf.data = ptr;
	return true;
}

static bool get_next_msg(struct ubus_socket *self, int *recv_fd)
{
	struct {
		struct ubus_msghdr hdr;
		struct blob_attr data;
	} hdrbuf;
	struct iovec iov = STATIC_IOV(hdrbuf);
	int len;
	int r;

	/* receive header + start attribute */
	r = recv_retry(self->sock.fd, &iov, false, recv_fd);
	if (r <= 0) {
		if (r < 0)
			self->sock.eof = true;

		return false;
	}

	if (!ubus_validate_hdr(&hdrbuf.hdr))
		return false;

	len = blob_attr_raw_len(&hdrbuf.data);
	if (!alloc_msg_buf(self, len))
		return false;

	memcpy(&self->msgbuf.hdr, &hdrbuf.hdr, sizeof(hdrbuf.hdr));
	memcpy(self->msgbuf.data, &hdrbuf.data, sizeof(hdrbuf.data));

	iov.iov_base = (char *)self->msgbuf.data + sizeof(hdrbuf.data);
	iov.iov_len = blob_attr_len(self->msgbuf.data);
	if (iov.iov_len > 0 && !recv_retry(self->sock.fd, &iov, true, NULL))
		return false;

	return true;
}
/*
static void ubus_refresh_state(struct ubus_socket *self){
	struct ubus_object *obj, *tmp;
	struct ubus_object **objs;
	int n, i = 0;

	//avl_for_each_element(&self->objects, obj, avl)
//		if (obj->type)
//			obj->type->id = 0;

	objs = alloca(self->objects.count * sizeof(*objs));
	avl_remove_all_elements(&self->objects, obj, avl, tmp) {
		objs[i++] = obj;
		obj->id = 0;
	}

	for (n = i, i = 0; i < n; i++)
		ubus_add_object(self, objs[i]);
}
static void _ubus_default_connection_lost(struct ubus_socket *self){
	//if (self->sock.registered)
	//		uloop_delete(&self->uloop);
}

*/

static void _ubus_process_pending_msg(struct uloop_timeout *timeout){
	//struct ubus_socket *self = container_of(timeout, struct ubus_socket, pending_timer);
	//struct ubus_pending_msg *pending;

/*
	while (!self->stack_depth && !list_empty(&self->pending)) {
		pending = list_first_entry(&self->pending, struct ubus_pending_msg, list);
		list_del(&pending->list);
		ubus_process_msg(self, &pending->hdr, -1);
		free(pending);
	}
	*/
}


//struct blob_buf b __hidden = {};
/*static int ubus_cmp_id(const void *k1, const void *k2, void *ptr){
	const uint32_t *id1 = k1, *id2 = k2;

	if (*id1 < *id2)
		return -1;
	else
		return *id1 > *id2;
}
*/

int ubus_reconnect(struct ubus_socket *self, const char *path){
	/*struct {
		struct ubus_msghdr hdr;
		struct blob_attr data;
	} hdr;
	*/
	//struct blob_attr *buf;
	int ret = UBUS_STATUS_UNKNOWN_ERROR;
	
	if (!path)
		path = UBUS_UNIX_SOCKET;

	if (self->sock.fd >= 0) {
		//if (self->sock.registered)
	//		uloop_remove_fd(self->uloop, &self->sock);

		close(self->sock.fd);
	}

	self->sock.fd = usock(USOCK_UNIX, path, NULL);
	if (self->sock.fd < 0)
		return UBUS_STATUS_CONNECTION_FAILED;

/*
	if (read(self->sock.fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		goto out_close;

	if (!ubus_validate_hdr(&hdr.hdr))
		goto out_close;

	if (hdr.hdr.type != UBUS_MSG_HELLO)
		goto out_close;

	buf = calloc(1, blob_attr_raw_len(&hdr.data));
	if (!buf)
		goto out_close;

	memcpy(buf, &hdr.data, sizeof(hdr.data));
	if (read(self->sock.fd, blob_attr_data(buf), blob_attr_len(buf)) != blob_attr_len(buf))
		goto out_free;

	self->local_id = hdr.hdr.peer;
	if (!self->local_id)
		goto out_free;
*/
	ret = UBUS_STATUS_OK;
	fcntl(self->sock.fd, F_SETFL, fcntl(self->sock.fd, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

//	ubus_refresh_state(self);

//out_free:
//	free(buf);
//out_close:
//	if (ret)
//		close(self->sock.fd);

	return ret;
}

int ubus_socket_connect(struct ubus_socket *self, const char *path){
	
/*
	self->sock.fd = -1;
	self->sock.cb = _ubus_handle_data;
	//self->connection_lost = _ubus_default_connection_lost;
	self->pending_timer.cb = _ubus_process_pending_msg;

	self->msgbuf.data = calloc(UBUS_MSG_CHUNK_SIZE, sizeof(char));
	if (!self->msgbuf.data)
		return -1;
	self->msgbuf_data_len = UBUS_MSG_CHUNK_SIZE;

	INIT_LIST_HEAD(&self->requests);
	INIT_LIST_HEAD(&self->pending);
	//avl_init(&self->objects, ubus_cmp_id, false, NULL);
	if (ubus_reconnect(self, path)) {
		free(self->msgbuf.data);
		return -1;
	}
*/
	return 0;
}
/*
static void ubus_auto_reconnect_cb(struct uloop_timeout *timeout)
{
	//struct ubus_auto_conn *conn = container_of(timeout, struct ubus_auto_conn, timer);

	fprintf(stderr, "%s: fix autoreconnect!\n", __FUNCTION__); 

	//if (!ubus_reconnect(&conn->self, conn->path))
	//	ubus_add_uloop(&conn->self);
	//else
	//	uloop_timeout_set(self->uloop, timeout, 1000);
}
*/
/*
static void ubus_auto_disconnect_cb(struct ubus_socket *self)
{
	struct ubus_auto_conn *conn = container_of(self, struct ubus_auto_conn, self);

	conn->timer.cb = ubus_auto_reconnect_cb;
	uloop_timeout_set(&conn->timer, 1000);
}
static void ubus_auto_connect_cb(struct uloop_timeout *timeout)
{
	struct ubus_auto_conn *conn = container_of(timeout, struct ubus_auto_conn, timer);

	if (ubus_connect(&conn->self, conn->path)) {
		uloop_timeout_set(timeout, 1000);
		fprintf(stderr, "failed to connect to ubus\n");
		return;
	}
	conn->self.connection_lost = ubus_auto_disconnect_cb;
	if (conn->cb)
		conn->cb(&conn->self);
	//ubus_add_uloop(&conn->self);
}

void ubus_auto_connect(struct ubus_auto_conn *conn)
{
	conn->timer.cb = ubus_auto_connect_cb;
	ubus_auto_connect_cb(&conn->timer);
}
*/
#endif
