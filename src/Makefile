OBJS =	iv_main.o			\
	iv_method_dev_poll.o		\
	iv_method_epoll.o		\
	iv_method_kqueue.o		\
	iv_method_poll.o		\
	iv_method_select.o		\
	iv_task.o			\
	iv_timer.o


all:		libivykis.a

clean:
		rm -f libivykis.a $(OBJS)

libivykis.a:	$(OBJS)
		$(AR) -rc libivykis.a $(OBJS)
		ranlib libivykis.a

.c.o:		iv.h iv_list.h iv_private.h
		$(CC) $(CFLAGS) -Wall -fomit-frame-pointer -c -o $@ $<
