HTML =	iv_event.3.html				\
	iv_event_post.3.html			\
	iv_event_raw.3.html			\
	iv_event_raw_post.3.html		\
	iv_event_raw_register.3.html		\
	iv_event_raw_unregister.3.html		\
	iv_event_register.3.html		\
	iv_event_unregister.3.html		\
	iv_popen.3.html				\
	iv_popen_request_close.3.html		\
	iv_popen_request_submit.3.html		\
	iv_signal.3.html			\
	iv_signal_register.3.html		\
	iv_signal_unregister.3.html		\
	iv_thread.3.html			\
	iv_thread_create.3.html			\
	iv_thread_set_debug_state.3.html	\
	iv_wait.3.html				\
	iv_wait_interest_register.3.html	\
	iv_wait_interest_unregister.3.html	\
	iv_work.3.html				\
	iv_work_pool_create.3.html		\
	iv_work_pool_put.3.html			\
	iv_work_pool_submit_work.3.html


all:		$(HTML)

clean:
		rm -f $(HTML)

%.3.html:	%.3
		cat $< | man2html -r | tail -n +3 > $@
