BUILD_DIR=build_dir
SOURCE=client.c 

TARGETS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=-fPIC -Wall -Werror -std=gnu99

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I/usr/include/lua5.1 -o $(patsubst %.o,%,$@) $^ -L../build_dir -lubus2 -lubox 

clean: 
	-rm -rf build_dir $(TARGETS)
