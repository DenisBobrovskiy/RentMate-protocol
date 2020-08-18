INC = -I../../misc -I../../protocol -I../../misc/mbedtls/include/
LIBDIR = -L ../../libs
LIBS = -lpthread -lpckData -larrayList -laesgcm

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

obj: server.c
	gcc $(INC) $(LIBDIR) -c *.c $(LIBS)

clean:
	rm *.o *.exe

