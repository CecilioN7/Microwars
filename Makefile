
# for sound, add the following libraries to the build.
# /usr/lib/x86_64-linux-gnu/libopenal.so /usr/lib/libalut.so
#
# like this:
#	g++ proj.cpp libggfonts.a -Wall -oproj -lX11 -lGL -lGLU -lm -lrt \
#	/usr/lib/x86_64-linux-gnu/libopenal.so \
#	/usr/lib/libalut.so
#

all: proj

proj: proj.cpp fonts.h
	g++ proj.cpp libggfonts.a -Wall -o proj -lX11 -lGL -lGLU -lm -lrt

clean:
	rm -f *.o

