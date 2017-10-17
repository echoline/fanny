LDFLAGS=-lfann -ljpeg -g
CFLAGS=-g

all: fann0 create

fann0: main.o jpeg.o comms.o
	gcc -o fann0 main.o jpeg.o comms.o ${LDFLAGS} -pthread

create: create.o
	gcc -o create create.o ${LDFLAGS}

clean:
	rm -f *.o create fann0


