all:		avl client connectfail connectreset null nullclose server server_thread timer timer_rat

clean:
		rm -f avl client connectfail connectreset null nullclose server server_thread timer timer_rat

avl:		avl.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o avl avl.c `../misc/ivykis-config --cflags --libs`

client:		client.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o client client.c `../misc/ivykis-config --cflags --libs`

connectfail:	connectfail.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o connectfail connectfail.c `../misc/ivykis-config --cflags --libs`

connectreset:	connectreset.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o connectreset connectreset.c `../misc/ivykis-config --cflags --libs`

null:		null.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o null null.c `../misc/ivykis-config --cflags --libs`

nullclose:	nullclose.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o nullclose nullclose.c `../misc/ivykis-config --cflags --libs`

server:		server.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o server server.c `../misc/ivykis-config --cflags --libs`

server_thread:	server.c ../src/libivykis.a
		gcc -DTHREAD -I../src -L../src -O3 -Wall -fomit-frame-pointer -o server_thread server.c `../misc/ivykis-config --cflags --libs`

timer:		timer.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o timer timer.c `../misc/ivykis-config --cflags --libs`

timer_rat:	timer_rat.c ../src/libivykis.a
		gcc -I../src -L../src -O3 -Wall -fomit-frame-pointer -o timer_rat timer_rat.c `../misc/ivykis-config --cflags --libs`

../src/libivykis.a:
		make -C ../src libivykis.a
