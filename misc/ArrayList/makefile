
#Compiles everything and packs into a static library
lib: obj
	ar -rcv libarrayList.a *.o
	mv libarrayList.a ../../libs

obj: arrayList.c arrayList.h
	gcc -c *.c
