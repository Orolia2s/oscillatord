src_dir := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))/src
VPATH := $(src_dir)
src_files = $(shell find $(src_dir) -type f -name '*.c')
warnings := -Wall \
	-Wextra \
	-Wformat=2 \
	-Wunused-variable \
	-Wno-unused-parameter \
	-Wmissing-declarations \
	-Wpointer-arith

all: $(NAME)

$(NAME): $(src_files)
	$(CC) $^ -o $@ $(CFLAGS) $(warnings) -lm -loscillator-disciplining -lspi2c -ltsync

clean:
	rm -f $(NAME)
