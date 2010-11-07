all:			iv_event_test iv_event_raw_test iv_popen_test iv_thread_test iv_signal_child_test iv_signal_test iv_wait_test iv_work_test

clean:
			rm -f iv_event_test iv_event_raw_test iv_popen_test iv_thread_test iv_signal_child_test iv_signal_test iv_wait_test iv_work_test

iv_event_test:		iv_event_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_event_test iv_event_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs` -lpthread

iv_event_raw_test:	iv_event_raw_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_event_raw_test iv_event_raw_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs`

iv_popen_test:		iv_popen_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_popen_test iv_popen_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs`

iv_signal_child_test:	iv_signal_child_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_signal_child_test iv_signal_child_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs`

iv_signal_test:		iv_signal_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_signal_test iv_signal_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs`

iv_thread_test:		iv_thread_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_thread_test iv_thread_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs` -lpthread

iv_wait_test:		iv_wait_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_wait_test iv_wait_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs` -lpthread

iv_work_test:		iv_work_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_work_test iv_work_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs` -lpthread

../libivykis-modules.a:
			make -C .. libivykis-modules.a

../../lib/libivykis.a:
			make -C ../../lib libivykis.a
