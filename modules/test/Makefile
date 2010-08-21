all:			iv_event_raw_test iv_signal_test

clean:
			rm -f iv_event_raw_test iv_signal_test

iv_event_raw_test:	iv_event_raw_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_event_raw_test iv_event_raw_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs`

iv_signal_test:		iv_signal_test.c ../libivykis-modules.a ../../lib/libivykis.a
			gcc -I../include -I../../lib/include/ -L.. -L../../lib/ -O3 -Wall -fomit-frame-pointer -o iv_signal_test iv_signal_test.c ../libivykis-modules.a `../../misc/ivykis-config --cflags --libs`

../libivykis-modules.a:
			make -C .. libivykis-modules.a

../../lib/libivykis.a:
			make -C ../../lib libivykis.a
