#Compiles everything and packs into a static library
lib: obj
	ar -rcv libtinyECDH.a *.o
	mv libtinyECDH.a ../../libs

obj: ecdh.c ecdh.h
	gcc -c *.c

