CFLAGS = -g3 -Wall -Wextra -Wno-unused-function

compile:
	mkdir -p build
	eval $(CC) -o build/dunsim_debug   -DDEBUG=1 -O0 $(CFLAGS) src/main_sdl3.c src/game.c `pkg-config sdl3 --cflags --libs`
	eval $(CC) -o build/dunsim_release -DDEBUG=0 -O2 $(CFLAGS) src/main_sdl3.c src/game.c `pkg-config sdl3 --cflags --libs`

run:
	build/dunsim_debug
