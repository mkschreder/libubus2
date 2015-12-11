BUILD_DIR=build_dir
SHARED_LIB=$(BUILD_DIR)/ubus.so 
SOURCE=ubus.c 

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=-fPIC -Wall -Werror -std=gnu99

all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(SHARED_LIB): $(OBJECTS) 
	$(CC) -shared -Wl,--no-undefined -o $@ $^ -L../build_dir -llua5.1 -lubus2 -lubox 

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I/usr/include/lua5.1 -c $^ -o $@

clean: 
	-rm $(SHARED_LIB) $(OBJECTS)
