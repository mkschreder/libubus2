/*
 * Copyright (C) 2011-2014 Felix Fietkau <nbd@openwrt.org>
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
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <libusys/usock.h>

#include <blobpack/blobpack.h>

#include "ubus_context.h"

#define STATIC_IOV(_var) { .iov_base = (char *) &(_var), .iov_len = sizeof(_var) }

#define UBUS_MSGBUF_REDUCTION_INTERVAL	16
/*
static const struct blob_attr_policy ubus_policy[UBUS_ATTR_MAX] = {
	[UBUS_ATTR_STATUS] = { .type = BLOB_ATTR_INT32 },
	[UBUS_ATTR_OBJID] = { .type = BLOB_ATTR_INT32 },
	[UBUS_ATTR_OBJPATH] = { .type = BLOB_ATTR_STRING },
	[UBUS_ATTR_METHOD] = { .type = BLOB_ATTR_STRING },
	[UBUS_ATTR_ACTIVE] = { .type = BLOB_ATTR_INT8 },
	[UBUS_ATTR_NO_REPLY] = { .type = BLOB_ATTR_INT8 },
	[UBUS_ATTR_SUBSCRIBERS] = { .type = BLOB_ATTR_ARRAY },
};

__hidden struct blob_attr **ubus_parse_msg(struct blob_attr *msg){
	
	blob_attr_parse(msg, attrbuf, ubus_policy, UBUS_ATTR_MAX);
	return attrbuf;
}
*/
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

int __hidden ubus_send_msg(struct ubus_context *ctx, uint32_t seq,
			   void *msg, size_t size, int cmd, uint32_t peer, int fd)
{
	struct ubus_msghdr hdr;
	struct iovec iov[2] = {
		STATIC_IOV(hdr)
	};
	int ret;

	printf("send msg %d %d %d\n", cmd, seq, peer); 

	hdr.version = 0;
	hdr.type = cmd;
	hdr.seq = seq;
	hdr.peer = peer;

	if (!msg) {
		blob_buf_reset(&ctx->buf);
		msg = blob_buf_head(&ctx->buf);
	}

	iov[1].iov_base = (char *) msg;
	iov[1].iov_len = size; ;

	ret = writev_retry(ctx->sock.fd, iov, ARRAY_SIZE(iov), fd);
	if (ret < 0)
		ctx->sock.eof = true;

	if (fd >= 0)
		close(fd);

	return ret;
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
			//if (ctx->cancelled)
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

static bool alloc_msg_buf(struct ubus_context *ctx, int len)
{
	void *ptr;
	int buf_len = ctx->msgbuf_data_len;
	int rem;

	if (!ctx->msgbuf.data)
		buf_len = 0;

	rem = (len % UBUS_MSG_CHUNK_SIZE);
	if (rem > 0)
		len += UBUS_MSG_CHUNK_SIZE - rem;

	if (len < buf_len &&
	    ++ctx->msgbuf_reduction_counter > UBUS_MSGBUF_REDUCTION_INTERVAL) {
		ctx->msgbuf_reduction_counter = 0;
		buf_len = 0;
	}

	if (len <= buf_len)
		return true;

	ptr = realloc(ctx->msgbuf.data, len);
	if (!ptr)
		return false;

	ctx->msgbuf.data = ptr;
	return true;
}

static bool get_next_msg(struct ubus_context *ctx, int *recv_fd)
{
	struct {
		struct ubus_msghdr hdr;
		struct blob_attr data;
	} hdrbuf;
	struct iovec iov = STATIC_IOV(hdrbuf);
	int len;
	int r;

	/* receive header + start attribute */
	r = recv_retry(ctx->sock.fd, &iov, false, recv_fd);
	if (r <= 0) {
		if (r < 0)
			ctx->sock.eof = true;

		return false;
	}

	if (!ubus_validate_hdr(&hdrbuf.hdr))
		return false;

	len = blob_attr_raw_len(&hdrbuf.data);
	if (!alloc_msg_buf(ctx, len))
		return false;

	memcpy(&ctx->msgbuf.hdr, &hdrbuf.hdr, sizeof(hdrbuf.hdr));
	memcpy(ctx->msgbuf.data, &hdrbuf.data, sizeof(hdrbuf.data));

	iov.iov_base = (char *)ctx->msgbuf.data + sizeof(hdrbuf.data);
	iov.iov_len = blob_attr_len(ctx->msgbuf.data);
	if (iov.iov_len > 0 && !recv_retry(ctx->sock.fd, &iov, true, NULL))
		return false;

	return true;
}

static void ubus_refresh_state(struct ubus_context *ctx){
	struct ubus_object *obj, *tmp;
	struct ubus_object **objs;
	int n, i = 0;

	/* clear all type IDs, they need to be registered again */
	avl_for_each_element(&ctx->objects, obj, avl)
		if (obj->type)
			obj->type->id = 0;

	/* push out all objects again */
	objs = alloca(ctx->objects.count * sizeof(*objs));
	avl_remove_all_elements(&ctx->objects, obj, avl, tmp) {
		objs[i++] = obj;
		obj->id = 0;
	}

	for (n = i, i = 0; i < n; i++)
		ubus_add_object(ctx, objs[i]);
}

static void _ubus_default_connection_lost(struct ubus_context *ctx){
	//if (ctx->sock.registered)
	//		uloop_delete(&ctx->uloop);
}


static void _ubus_process_pending_msg(struct uloop_timeout *timeout){
	struct ubus_context *ctx = container_of(timeout, struct ubus_context, pending_timer);
	struct ubus_pending_msg *pending;

	while (!ctx->stack_depth && !list_empty(&ctx->pending)) {
		pending = list_first_entry(&ctx->pending, struct ubus_pending_msg, list);
		list_del(&pending->list);
		ubus_process_msg(ctx, &pending->hdr, -1);
		free(pending);
	}
}


static void _ubus_handle_data(struct uloop_fd *u, unsigned int events){
	struct ubus_context *ctx = container_of(u, struct ubus_context, sock);
	int recv_fd = -1;

	while (get_next_msg(ctx, &recv_fd)) {
		ubus_process_msg(ctx, &ctx->msgbuf, recv_fd);
		//if (uloop_cancelled)
		//	break;
	}

	if (u->eof)
		ctx->connection_lost(ctx);
}

//struct blob_buf b __hidden = {};
static int ubus_cmp_id(const void *k1, const void *k2, void *ptr){
	const uint32_t *id1 = k1, *id2 = k2;

	if (*id1 < *id2)
		return -1;
	else
		return *id1 > *id2;
}


void  ubus_poll_data(struct ubus_context *ctx, int timeout){
	struct pollfd pfd = {
		.fd = ctx->sock.fd,
		.events = POLLIN | POLLERR,
	};

	poll(&pfd, 1, timeout);
	_ubus_handle_data(&ctx->sock, ULOOP_READ);
}

int ubus_connect(struct ubus_context *ctx, const char *path)
{
	ctx->sock.fd = -1;
	ctx->sock.cb = _ubus_handle_data;
	ctx->connection_lost = _ubus_default_connection_lost;
	ctx->pending_timer.cb = _ubus_process_pending_msg;

	ctx->msgbuf.data = calloc(UBUS_MSG_CHUNK_SIZE, sizeof(char));
	if (!ctx->msgbuf.data)
		return -1;
	ctx->msgbuf_data_len = UBUS_MSG_CHUNK_SIZE;

	INIT_LIST_HEAD(&ctx->requests);
	INIT_LIST_HEAD(&ctx->pending);
	avl_init(&ctx->objects, ubus_cmp_id, false, NULL);
	if (ubus_reconnect(ctx, path)) {
		free(ctx->msgbuf.data);
		return -1;
	}

	return 0;
}


int ubus_reconnect(struct ubus_context *ctx, const char *path){
	struct {
		struct ubus_msghdr hdr;
		struct blob_attr data;
	} hdr;
	struct blob_attr *buf;
	int ret = UBUS_STATUS_UNKNOWN_ERROR;

	if (!path)
		path = UBUS_UNIX_SOCKET;

	if (ctx->sock.fd >= 0) {
		if (ctx->sock.registered)
			uloop_remove_fd(ctx->uloop, &ctx->sock);

		close(ctx->sock.fd);
	}

	ctx->sock.fd = usock(USOCK_UNIX, path, NULL);
	if (ctx->sock.fd < 0)
		return UBUS_STATUS_CONNECTION_FAILED;

	if (read(ctx->sock.fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		goto out_close;

	if (!ubus_validate_hdr(&hdr.hdr))
		goto out_close;

	if (hdr.hdr.type != UBUS_MSG_HELLO)
		goto out_close;

	buf = calloc(1, blob_attr_raw_len(&hdr.data));
	if (!buf)
		goto out_close;

	memcpy(buf, &hdr.data, sizeof(hdr.data));
	if (read(ctx->sock.fd, blob_attr_data(buf), blob_attr_len(buf)) != blob_attr_len(buf))
		goto out_free;

	ctx->local_id = hdr.hdr.peer;
	if (!ctx->local_id)
		goto out_free;

	ret = UBUS_STATUS_OK;
	fcntl(ctx->sock.fd, F_SETFL, fcntl(ctx->sock.fd, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

	ubus_refresh_state(ctx);

out_free:
	free(buf);
out_close:
	if (ret)
		close(ctx->sock.fd);

	return ret;
}

static void ubus_auto_reconnect_cb(struct uloop_timeout *timeout)
{
	//struct ubus_auto_conn *conn = container_of(timeout, struct ubus_auto_conn, timer);

	fprintf(stderr, "%s: fix autoreconnect!\n", __FUNCTION__); 

	//if (!ubus_reconnect(&conn->ctx, conn->path))
	//	ubus_add_uloop(&conn->ctx);
	//else
	//	uloop_timeout_set(self->uloop, timeout, 1000);
}

static void ubus_auto_disconnect_cb(struct ubus_context *ctx)
{
	struct ubus_auto_conn *conn = container_of(ctx, struct ubus_auto_conn, ctx);

	conn->timer.cb = ubus_auto_reconnect_cb;
	uloop_timeout_set(&conn->timer, 1000);
}

static void ubus_auto_connect_cb(struct uloop_timeout *timeout)
{
	struct ubus_auto_conn *conn = container_of(timeout, struct ubus_auto_conn, timer);

	if (ubus_connect(&conn->ctx, conn->path)) {
		uloop_timeout_set(timeout, 1000);
		fprintf(stderr, "failed to connect to ubus\n");
		return;
	}
	conn->ctx.connection_lost = ubus_auto_disconnect_cb;
	if (conn->cb)
		conn->cb(&conn->ctx);
	//ubus_add_uloop(&conn->ctx);
}

void ubus_auto_connect(struct ubus_auto_conn *conn)
{
	conn->timer.cb = ubus_auto_connect_cb;
	ubus_auto_connect_cb(&conn->timer);
}

