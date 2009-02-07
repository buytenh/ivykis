all:		example

clean:
		rm -f example

example:	example.c
		gcc -Wall -o example example.c iv_inotify.c -livykis \
			-I../../src -L../../src
