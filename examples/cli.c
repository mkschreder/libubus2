#include <unistd.h>
#include <blobpack/blobpack.h>
#include <libusys/ustream.h>
#include <libutype/utils.h>

#include "../src/libubus2.h"

static int done = 0; 

static int usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [<options>] <command> [arguments...]\n"
		"Options:\n"
		" -s <socket>:		Set the unix domain socket to connect to\n"
		" -t <timeout>:		Set the timeout (in seconds) for a command to complete\n"
		" -S:			Use simplified output (for scripts)\n"
		" -v:			More verbose output\n"
		"\n"
		"Commands:\n"
		" - list [<path>]			List objects\n"
		" - call <client> <path> <method> [<message>]	Call an object method\n"
		" - listen [<path>...]			Listen for events\n"
		" - send <type> [<message>]		Send an event\n"
		" - wait_for <object> [<object>...]	Wait for multiple objects to appear on ubus\n"
		"\n", prog);
	done = 1; 
	return 1;
}

void _on_call_done(struct ubus_request *req, struct blob_attr *res){
	blob_attr_dump_json(res); 
	done = 1; 
}

void _on_list_done(struct ubus_request *req, struct blob_attr *res){
	if(!blob_attr_validate(res, "s[sa]")) {
		fprintf(stderr, "Invalid data returned from server!\n"); 
		blob_attr_dump_json(res); 
		return; 
	}
	
	struct blob_attr *key, *value; 
	blob_attr_for_each_kv(res, key, value){
		printf("%s\n", blob_attr_get_string(key)); 
		struct blob_attr *mname, *margs; 
		blob_attr_for_each_kv(value, mname, margs){
			char sig[255] = {0}, sig_ret[255] = {0}; 
			struct blob_attr *arg, *fields[3]; 
			blob_attr_for_each_child(margs, arg){
				blob_attr_parse(arg, "iss", fields, 3); 	
				switch(blob_attr_get_i8(fields[0])){
					case UBUS_METHOD_PARAM_IN: strncat(sig, blob_attr_get_string(fields[2]), sizeof(sig)); break; 
					case UBUS_METHOD_PARAM_OUT: strncat(sig_ret, blob_attr_get_string(fields[2]), sizeof(sig_ret)); break; 
				}
			}
			if(strlen(sig) == 0) strcpy(sig, "void"); 
			if(strlen(sig_ret) == 0) strcpy(sig_ret, "void"); 
			printf("\t%-30s(ARGS: %s, RETURN: %s)\n", blob_attr_get_string(mname), sig, sig_ret); 	
			blob_attr_for_each_child(margs, arg){
				blob_attr_parse(arg, "iss", fields, 3); 	
				printf("\t\t"); 
				if(blob_attr_get_i8(fields[0]) == UBUS_METHOD_PARAM_IN) printf("IN: "); 
				else if(blob_attr_get_i8(fields[0]) == UBUS_METHOD_PARAM_OUT) printf("OUT: "); 
				printf("%s '%s'\n", blob_attr_get_string(fields[1]), blob_attr_get_string(fields[2])); 
			}
		}
	}
	done = 1; 
}

void _on_request_failed(struct ubus_request *req, struct blob_attr *res){
	printf("request failed!\n"); 
	done = 1; 
}

static int _command_list(struct ubus_context *ctx, int argc, char **argv){
	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 
	struct ubus_request *req = ubus_request_new("ubus", "/ubus/server", "ubus.server.list", blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_list_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(ctx, &req); 
	blob_buf_free(&buf); 

	return 0; 
}

static int _command_call(struct ubus_context *ctx, int argc, char **argv){
	struct blob_buf buf; 
	blob_buf_init(&buf, 0, 0); 
	if(argc < 3) return usage("prog"); 
	if(argc == 4)
		blob_buf_add_json_from_string(&buf, argv[3]); 
	struct ubus_request *req = ubus_request_new(argv[0], argv[1], argv[2], blob_buf_head(&buf)); 
	ubus_request_on_resolve(req, &_on_call_done); 
	ubus_request_on_reject(req, &_on_request_failed); 
	ubus_send_request(ctx, &req); 
	blob_buf_free(&buf); 
	return 0; 
}

struct {
	const char *name;
	int (*cb)(struct ubus_context *ctx, int argc, char **argv);
} commands[] = {
	{ "list", _command_list },
	{ "call", _command_call },
	//{ "listen", ubus_cli_listen },
	//{ "send", ubus_cli_send },
};


int main(int argc, char **argv)
{
	const char *progname, *ubus_socket = "./ubus.sock";
	int verbose = 0, simple_output = 0; 
	char *cmd;
	int ret = 0;
	int i, ch;

	progname = argv[0];

	signal(SIGPIPE, SIG_IGN); 

	while ((ch = getopt(argc, argv, "vs:t:S")) != -1) {
		switch (ch) {
		case 's':
			ubus_socket = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			return usage(progname);
		}
	}

	argc -= optind;
	argv += optind;

	cmd = argv[0];
	if (argc < 1)
		return usage(progname);

	struct ubus_context *ctx = ubus_new("ubus-cli"); 
	if(ubus_connect(ctx, ubus_socket) < 0){
		if (!simple_output)
			fprintf(stderr, "Failed to connect to ubus\n");
		return -1;
	}

	argv++;
	argc--;

	ret = -2;
	for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
		if (strcmp(commands[i].name, cmd) != 0)
			continue;

		ret = commands[i].cb(ctx, argc, argv);
		break;
	}

	while(!done){
		ubus_handle_events(ctx); 
	}
	
	if (ret != 0)
		fprintf(stderr, "Command failed!");
	else if (ret == -2)
		usage(progname);

	ubus_delete(&ctx);
	return ret;
}

