OBJS =	iv_event_raw.o


all:			libivykis-modules.a

clean:
			rm -f libivykis-modules.a $(OBJS)
			make -C man3 clean
			make -C test clean

libivykis-modules.a:	$(OBJS)
			$(AR) -rc libivykis-modules.a $(OBJS)
			ranlib libivykis-modules.a

.c.o:			include/iv_event_raw.h
			$(CC) $(CFLAGS) -D_GNU_SOURCE -Wall -Iinclude/ -I../lib/include/ -fomit-frame-pointer -c -o $@ $<
