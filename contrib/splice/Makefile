all:		discard echo

clean:
		rm -f discard echo

discard:	discard.c
		gcc -Wall -o discard discard.c -livykis \
			-I../../src -L../../src

echo:		echo.c
		gcc -Wall -o echo echo.c -livykis \
			-I../../src -L../../src
