ssu_mntr : ssu_mntr.o
	gcc ssu_mntr.o -o ssu_mntr

ssu_mntr.o : ssu_mntr.c
	gcc -c ssu_mntr.c

clean :
	rm *.o
	rm ssu_mntr
