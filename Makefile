BUILD_DIR=build_dir
STATIC_LIB=libubus2.a
SHARED_LIB=libubus2.so 
SOURCE=\
	src/ubus_context.c \
	src/ubus_client.c \
	src/ubus_method.c \
	src/ubus_object.c \
	src/ubus_method.c \
	src/ubus_id.c \
	src/ubus_peer.c \
	src/ubus_request.c \
	src/ubus_server.c \
	src/ubus_rawsocket.c \
	src/ubus_proxy.c \
	src/libubus2.c \
	sockets/json_websocket.c \
	sockets/json_socket.c

INSTALL_PREFIX:=$(DESTDIR)/usr/

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=-g -Isrc -Wall -Werror -std=gnu99 -Wmissing-field-initializers
LDFLAGS+=-lblobpack -lusys -lutype -ljson-c -ldl -lwebsockets -lm

all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB) ubus1-example cli-example socket-example client-example threads-example websocket-example

#extras: 
#	make -C lua 

.PHONY: extras

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(SHARED_LIB): $(OBJECTS) 
	$(CC) -shared -fPIC -Wl,--no-undefined -o $@ $^ $(LDFLAGS)

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs -o $@ $^

threads-example: examples/threads.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ examples/threads.o $(LDFLAGS) -L$(BUILD_DIR) $(OBJECTS) -lpthread

client-example: examples/client.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ examples/client.o $(LDFLAGS) -L$(BUILD_DIR) $(OBJECTS) -lpthread

socket-example: examples/socket.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ examples/socket.o $(LDFLAGS) -L$(BUILD_DIR) $(OBJECTS) -lpthread

cli-example: examples/cli.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ examples/cli.o $(LDFLAGS) -L$(BUILD_DIR) $(OBJECTS) -lpthread

ubus1-example: examples/ubus1_proxy.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ examples/ubus1_proxy.o $(LDFLAGS) -L$(BUILD_DIR) $(OBJECTS) -lpthread

websocket-example: examples/websocket.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ examples/websocket.o $(LDFLAGS) -L$(BUILD_DIR) $(OBJECTS) -lpthread

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -fPIC $(CFLAGS) -c $^ -o $@

install: 
	mkdir -p $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/include/libubus2/
	cp -R $(SHARED_LIB) $(STATIC_LIB) $(INSTALL_PREFIX)/lib
	cp -R src/*.h sockets/*.h $(INSTALL_PREFIX)/include/libubus2/
clean: 
	rm -rf build_dir
	rm -f examples/*.o
