
static void
ubus_process_unsubscribe(struct ubus_context *ctx, struct ubus_msghdr *hdr,
			 struct ubus_object *obj, struct blob_attr **attrbuf)
{
	struct ubus_subscriber *s;

	if (!obj || !attrbuf[UBUS_ATTR_TARGET])
		return;

	s = container_of(obj, struct ubus_subscriber, obj);
	
	//if (obj->methods != s->watch_method)
	//	return;

	if (s->remove_cb)
		s->remove_cb(ctx, s, blob_attr_get_u32(attrbuf[UBUS_ATTR_TARGET]));
}

