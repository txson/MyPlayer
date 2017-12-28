inc = ./include
lib = ./lib

my_play : *.c
	gcc *.c -I$(inc) -lavcodec -lavformat -lswscale -lavutil -lavutil -lswresample -L$(lib) -g -o my_play -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT -L/usr/lib/x86_64-linux-gnu -lSDL

.PHONY : clean
clean : 
	rm my_play
