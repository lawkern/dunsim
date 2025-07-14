CFLAGS = -g3 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
LDLIBS = -lm

compile:
	mkdir -p build
	eval $(CC) -o build/dunsim_debug   -DDEBUG=1 -O0 $(CFLAGS) src/main_sdl3.c `pkg-config sdl3 --cflags --libs` $(LDLIBS)
	eval $(CC) -o build/dunsim_release -DDEBUG=0 -O2 $(CFLAGS) src/main_sdl3.c `pkg-config sdl3 --cflags --libs` $(LDLIBS)

run:
	build/dunsim_debug
