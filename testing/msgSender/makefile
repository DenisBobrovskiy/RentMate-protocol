INC = -I/home/main/CProjects/include
LIBDIR = -L/home/main/CProjects/libs
LIBS = -laesgcm -lpckData

run: all
	./msgSender.exe

all: obj
	gcc -o msgSender.exe *.o $(INC) $(LIBDIR) $(LIBS)

gdb: obj
	gcc -o msgSender.exe *.o $(INC) $(LIBDIR) $(LIBS) -g && ./msgSender.exe

obj: msgSender.c commandHandlesReserved.c
	gcc -c *.c $(INC)

