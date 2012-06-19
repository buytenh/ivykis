if HAVE_INOTIFY
INOTIFY_MAN	= iv_inotify.3				\
		  IV_INOTIFY_INIT.3			\
		  iv_inotify_register.3			\
		  iv_inotify_unregister.3		\
		  IV_INOTIFY_WATCH_INIT.3		\
		  iv_inotify_watch_register.3		\
		  iv_inotify_watch_unregister.3
else
INOTIFY_MAN	=
endif

man3_MANS	= iv_event.3				\
		  IV_EVENT_INIT.3			\
		  iv_event_post.3			\
		  iv_event_raw.3			\
		  IV_EVENT_RAW_INIT.3			\
		  iv_event_raw_post.3			\
		  iv_event_raw_register.3		\
		  iv_event_raw_unregister.3		\
		  iv_event_register.3			\
		  iv_event_unregister.3			\
		  iv_fd_pump.3				\
		  iv_fd_pump_destroy.3			\
		  IV_FD_PUMP_INIT.3			\
		  iv_fd_pump_init.3			\
		  iv_fd_pump_is_done.3			\
		  iv_fd_pump_pump.3			\
		  $(INOTIFY_MAN)			\
		  iv_popen.3				\
		  iv_popen_request_close.3		\
		  IV_POPEN_REQUEST_INIT.3		\
		  iv_popen_request_submit.3		\
		  iv_signal.3				\
		  IV_SIGNAL_INIT.3			\
		  iv_signal_register.3			\
		  iv_signal_unregister.3		\
		  iv_thread.3				\
		  iv_thread_create.3			\
		  iv_thread_set_debug_state.3		\
		  iv_wait.3				\
		  IV_WAIT_INTEREST_INIT.3		\
		  iv_wait_interest_register.3		\
		  iv_wait_interest_register_spawn.3	\
		  iv_wait_interest_unregister.3		\
		  iv_work.3				\
		  IV_WORK_ITEM_INIT.3			\
		  iv_work_pool_create.3			\
		  IV_WORK_POOL_INIT.3			\
		  iv_work_pool_put.3			\
		  iv_work_pool_submit_work.3

EXTRA_DIST	= ${man3_MANS}
