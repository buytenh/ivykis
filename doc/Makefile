HTML =	iv_examples.3.html		\
	iv_fd.3.html			\
	iv_fd_set_handler_err.3.html	\
	iv_fd_set_handler_in.3.html	\
	iv_fd_set_handler_out.3.html	\
	iv_init.3.html			\
	iv_invalidate_now.3.html	\
	iv_main.3.html			\
	iv_quit.3.html			\
	iv_register_fd.3.html		\
	iv_register_task.3.html		\
	iv_register_timer.3.html	\
	iv_task.3.html			\
	iv_time.3.html			\
	iv_timer.3.html			\
	iv_unregister_fd.3.html		\
	iv_unregister_task.3.html	\
	iv_unregister_timer.3.html	\
	iv_validate_now.3.html		\
	ivykis.3.html


all:		$(HTML)

clean:
		rm -f $(HTML)

%.3.html:	%.3
		cat $< | man2html -r | tail +3 > $@
