all:		src/libivykis.a test/client

clean:
		$(MAKE) -C src clean
		$(MAKE) -C test clean

src/libivykis.a:
		$(MAKE) -C src

test/client:
		$(MAKE) -C test
