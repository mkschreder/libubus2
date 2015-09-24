BUILD_DIR=build_dir
STATIC_LIB=$(BUILD_DIR)/libubus2.a
SHARED_LIB=$(BUILD_DIR)/libubus2.so 
SOURCE=\
	src/ubus_context.c \
	src/ubus_method.c \
	src/ubus_object.c \
	src/ubus_object_type.c \
	src/ubus_request.c \
	src/ubus_socket.c \
	src/ubus_subscriber.c \
	src/libubus2.c 

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=-fPIC -Wall -Werror -std=gnu99

all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB) extras 

extras: 
	make -C lua 
	make -C examples 

.PHONY: extras

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(SHARED_LIB): $(OBJECTS) 
	$(CC) -shared -Wl,--no-undefined -o $@ $^ -lubox -lblobmsg_json -ldl

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $^ -o $@

clean: 
	make -C lua clean 
	rm -rf build_dir
