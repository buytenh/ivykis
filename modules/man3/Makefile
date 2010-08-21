HTML =	iv_event_raw.3.html			\
	iv_event_raw_post.3.html		\
	iv_event_raw_register.3.html		\
	iv_event_raw_unregister.3.html		\
	iv_signal.3.html			\
	iv_signal_register.3.html		\
	iv_signal_unregister.3.html


all:		$(HTML)

clean:
		rm -f $(HTML)

%.3.html:	%.3
		cat $< | man2html -r | tail -n +3 > $@
