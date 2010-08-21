all:		lib/libivykis.a lib/test/client modules/libivykis-modules.a modules/test/iv_event_raw_test

clean:
		$(MAKE) -C lib clean
		$(MAKE) -C modules clean

lib/libivykis.a:
		$(MAKE) -C lib

lib/test/client:
		$(MAKE) -C lib/test

modules/libivykis-modules.a:
		$(MAKE) -C modules

modules/test/iv_event_raw_test:
		$(MAKE) -C modules/test
