all:		kojines

clean:
		rm -f kojines

kojines:	main.c kojines.c
		gcc -Wall -o kojines main.c kojines.c -livykis -lrt \
			-I../../lib -L../../lib
