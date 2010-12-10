AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include

AM_CFLAGS		= -Wall

LDADD			= ../libivykis.la

noinst_PROGRAMS		= avl				\
			  client			\
			  connectfail			\
			  connectreset 			\
			  null				\
			  server			\
			  server_thread			\
			  timer				\
			  timer_rat

avl_SOURCES		= avl.c
client_SOURCES		= client.c
connectfail_SOURCES	= connectfail.c
connectreset_SOURCES	= connectreset.c
null_SOURCES		= null.c
server_SOURCES		= server.c
timer_SOURCES		= timer.c
timer_rat_SOURCES	= timer_rat.c

server_thread_CPPFLAGS	= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -DTHREAD
server_thread_SOURCES	= server.c
