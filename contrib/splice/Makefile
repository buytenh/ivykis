all:		discard echo

clean:
		rm -f discard echo

discard:	discard.c
		gcc -Wall -o discard discard.c -livykis -lrt \
			-I../../lib/include -L../../lib

echo:		echo.c
		gcc -Wall -o echo echo.c -livykis -lrt \
			-I../../lib/include -L../../lib
