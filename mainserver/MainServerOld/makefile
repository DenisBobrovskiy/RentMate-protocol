INC = -I../../misc 
LIBDIR = -L../../misc/mbedtls/crypto/library/ -L../../misc/ArrayList -L../../misc/AES-GCM
LIBS = -larrayList -laesgcm -lpthread -lpckData

all: server.exe

run: server.exe
	./server.exe

gdb: gdb.exe
	gdb ./server.exe

gdb.exe: obj
	gcc $(INC) $(LIBDIR) -o server.exe *.o $(LIBS) -g

#NORMAL BUILD
server.exe: obj
	gcc $(INC) $(LIBDIR) -o server.exe *.o $(LIBS)

obj: server.c commandHandlesServer.c
	gcc $(INC) $(LIBDIR) -c *.c $(LIBS)

clean:
	rm *.o *.exe

