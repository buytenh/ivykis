OBJS =	iv_event.o		\
	iv_event_raw.o		\
	iv_signal.o		\
	iv_wait.o


all:			libivykis-modules.a

clean:
			rm -f libivykis-modules.a $(OBJS)
			make -C man3 clean
			make -C test clean

libivykis-modules.a:	$(OBJS)
			$(AR) -rc libivykis-modules.a $(OBJS)
			ranlib libivykis-modules.a

.c.o:			include/iv_event.h include/iv_event_raw.h include/iv_signal.h include/iv_wait.h thr.h
			$(CC) $(CFLAGS) -D_GNU_SOURCE -Wall -Iinclude/ -I../lib/include/ -fomit-frame-pointer -c -o $@ $<
