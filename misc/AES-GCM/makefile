INC = -I../mbedtls/include/
LIBDIR = -L../mbedtls/library
LIBS = -lmbedcrypto

#Compile all and move to esp-idf projects
lib: obj
	ar -x ../mbedtls/library/libmbedcrypto.a
	ar -rc libaesgcm.a *.o
	rm *.o
	mv libaesgcm.a ../../libs


run: obj
	gcc $(INC) -o test.exe aes-gcm.c $(LIBDIR) $(LIBS) && ./test.exe


obj: aes-gcm.c aes-gcm.h
	gcc $(INC) -c aes-gcm.c
