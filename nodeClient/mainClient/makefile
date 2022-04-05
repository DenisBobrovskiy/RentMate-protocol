INC = -I../../misc/AES-GCM -I../../misc/ArrayList -I../../protocol/pckData -I../../misc/tinyECDH -I../../misc/mbedtls/include/mbedtls/
LIBDIR = -L ../../libs
LIBS = -lpthread -lpckData -larrayList -laesgcm -ltinyECDH

all: client.exe

run: client.exe
	./client.exe

gdb: gdb.exe
	gdb ./client.exe

gdb.exe: obj
	gcc $(INC) $(LIBDIR) -o client.exe *.o $(LIBS) -g

#NORMAL BUILD
client.exe: obj
	gcc $(INC) $(LIBDIR) -o client.exe *.o $(LIBS) 

obj: client.c
	gcc $(INC) $(LIBDIR) -c *.c ./commands/sharedCommands/sharedCommands.c ./commands/smartLock/commands.c $(LIBS)

clean:
	rm *.o *.exe

