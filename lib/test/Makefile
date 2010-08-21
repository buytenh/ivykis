all:		avl client connectfail connectreset null server server_thread timer timer_rat

clean:
		rm -f avl client connectfail connectreset null server server_thread timer timer_rat

avl:		avl.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o avl avl.c `../../misc/ivykis-config --cflags --libs`

client:		client.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o client client.c `../../misc/ivykis-config --cflags --libs`

connectfail:	connectfail.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o connectfail connectfail.c `../../misc/ivykis-config --cflags --libs`

connectreset:	connectreset.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o connectreset connectreset.c `../../misc/ivykis-config --cflags --libs`

null:		null.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o null null.c `../../misc/ivykis-config --cflags --libs`

server:		server.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o server server.c `../../misc/ivykis-config --cflags --libs`

server_thread:	server.c ../libivykis.a
		gcc -DTHREAD -I../include -L.. -O3 -Wall -fomit-frame-pointer -o server_thread server.c `../../misc/ivykis-config --cflags --libs` -lpthread

timer:		timer.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o timer timer.c `../../misc/ivykis-config --cflags --libs`

timer_rat:	timer_rat.c ../libivykis.a
		gcc -I../include -L.. -O3 -Wall -fomit-frame-pointer -o timer_rat timer_rat.c `../../misc/ivykis-config --cflags --libs`

../libivykis.a:
		make -C .. libivykis.a
