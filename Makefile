CFLAGS = -g3 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-multichar -Wno-missing-braces -fno-strict-aliasing
LDLIBS = -lm -lGL

compile:
	$(CC) -o build/ctime src/external/ctime.c

	build/ctime -begin build/debug.ctm
	eval $(CC) -o build/dunsim_debug   -DDEBUG=1 -O0 $(CFLAGS) src/main_sdl3.c $$(pkg-config sdl3 --cflags --libs) $(LDLIBS)
	build/ctime -end build/debug.ctm

	build/ctime -begin build/release.ctm
	eval $(CC) -o build/dunsim_release -DDEBUG=0 -O2 $(CFLAGS) src/main_sdl3.c $$(pkg-config sdl3 --cflags --libs) $(LDLIBS)
	build/ctime -end build/release.ctm

run:
	build/dunsim_debug

debug:
	gdb build/dunsim_debug
