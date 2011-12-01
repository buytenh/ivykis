AM_CPPFLAGS		= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include

AM_CFLAGS		= -Wall

LDADD			= ../libivykis.a

noinst_PROGRAMS		= avl				\
			  client			\
			  connectfail			\
			  connectreset 			\
			  null				\
			  server			\
			  server_thread			\
			  struct_sizes			\
			  timer				\
			  timer_rat

avl_SOURCES		= avl.c
client_SOURCES		= client.c
connectfail_SOURCES	= connectfail.c
connectreset_SOURCES	= connectreset.c
null_SOURCES		= null.c
server_SOURCES		= server.c
struct_sizes_SOURCES	= struct_sizes.c
timer_SOURCES		= timer.c
timer_rat_SOURCES	= timer_rat.c

server_thread_CPPFLAGS	= -D_GNU_SOURCE -I$(top_srcdir)/lib/include -I$(top_builddir)/lib/include -DTHREAD
server_thread_SOURCES	= server.c
