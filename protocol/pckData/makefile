INC = -I../../misc -I../../misc/mbedtls/include/
LIBDIR = -L ../../libs
LIBS = -lpckData -larrayList -laesgcm

run: all
	cd test && ./pckDataTest.exe

gdb: obj
	(gcc -o test/pckDataTest.exe ./test/*.o $(LIBDIR) $(LIBS) -g) && gdb ./test/pckDataTest.exe

all: lib
	gcc -c test/testPckData.c -o test/testPckData.o $(INC)
	gcc -o test/pckDataTest.exe test/*.o $(LIBDIR) $(LIBS)
	rm test/*.o

obj:
	 gcc -c *.c $(INC) 

lib: obj
	ar -x ../../libs/libarrayList.a
	ar -x ../../libs/libaesgcm.a
	ar -rc libpckData.a *.o 
	rm *.o
	mv libpckData.a ../../libs
