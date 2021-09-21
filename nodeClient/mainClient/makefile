INC = -I../../misc -I../../protocol -I../../misc/mbedtls/include/
LIBDIR = -L ../../libs
LIBS = -lpthread -lpckData -larrayList -laesgcm

all: client.exe

run: client.exe
	./client.exe

gdb: gdb.exe
	gdb ./client.exe

gdb.exe: obj
	gcc $(INC) $(LIBDIR) -o client.exe *.o $(LIBS) -g

#NORMAL BUILD
client.exe: obj
	gcc $(INC) $(LIBDIR) -o client.exe *.o $(LIBS) && cp ./client.c ./client.h ./socketThread.c ./socketThread.h ../testEsp32/main/

obj: client.c
	gcc $(INC) $(LIBDIR) -c *.c $(LIBS)

clean:
	rm *.o *.exe

