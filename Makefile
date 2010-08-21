all:		lib/libivykis.a test/client

clean:
		$(MAKE) -C lib clean
		$(MAKE) -C test clean

lib/libivykis.a:
		$(MAKE) -C lib

test/client:
		$(MAKE) -C test
