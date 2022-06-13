prog: parking.o
	gcc -o parking parking.o -pthread
	make clean

parking.o: parking.c
	gcc -o parking.o -c parking.c

clean :
	-rm *.o $(objects) 