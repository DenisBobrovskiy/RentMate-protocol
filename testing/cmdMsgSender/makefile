INC = -I/home/main/CProjects/include
LIBDIR = -L/home/main/CProjects/libs
LIBS = -laesgcm

run: all
	./cmdMsgSender.exe

all: obj
	gcc -o cmdMsgSender.exe *.o $(LIBDIR) $(LIBS)
obj:
	gcc -c *.c $(INC)
