BUILD_DIR=build_dir
STATIC_LIB=libubus2.a
SHARED_LIB=libubus2.so 
SOURCE=\
	src/ubus_message.c \
	src/ubus_id.c \
	src/ubus_srv_ws.c \
	src/ubus_cli_js.c 

INSTALL_PREFIX:=$(DESTDIR)/usr/

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=-g -Isrc -Wall -Werror -std=gnu99 -Wmissing-field-initializers
LDFLAGS+=-lblobpack -lusys -lutype -ldl -lpthread -lwebsockets -lm

all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB) \
	websocket-example
#	ubus1-example \
#	cli-example \
#	socket-example \
#	client-example \
#	threads-example \

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
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ $(OBJECTS) examples/threads.o $(LDFLAGS) -L$(BUILD_DIR) -lpthread

client-example: examples/client.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ $(OBJECTS) examples/client.o $(LDFLAGS) -L$(BUILD_DIR) -lpthread

socket-example: examples/socket.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ $(OBJECTS) examples/socket.o $(LDFLAGS) -L$(BUILD_DIR) -lpthread

cli-example: examples/cli.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ $(OBJECTS) examples/cli.o $(LDFLAGS) -L$(BUILD_DIR) -lpthread

ubus1-example: examples/ubus1_proxy.o $(OBJECTS)
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ $(OBJECTS) examples/ubus1_proxy.o $(LDFLAGS) -L$(BUILD_DIR) -lpthread

websocket-example: examples/websocket.o src/ubus_id.o src/ubus_message.o src/ubus_srv_ws.o 
	$(CC) -I$(shell pwd) $(CFLAGS) -o $@ $^ $(LDFLAGS) -L$(BUILD_DIR) -lpthread

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -fPIC $(CFLAGS) -c $^ -o $@

install: 
	mkdir -p $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/include/libubus2/
	cp -R $(SHARED_LIB) $(STATIC_LIB) $(INSTALL_PREFIX)/lib
	cp -R src/*.h $(INSTALL_PREFIX)/include/libubus2/
clean: 
	rm -rf build_dir
	rm -f examples/*.o
