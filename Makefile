all:		lib/libivykis.a lib/test/client

clean:
		$(MAKE) -C lib clean

lib/libivykis.a:
		$(MAKE) -C lib

lib/test/client:
		$(MAKE) -C lib/test
