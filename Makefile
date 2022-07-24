LDFLAGS=-lm -g -fopenmp -Wall `pkg-config --libs fann` `pkg-config --libs libjpeg` -lX11 -lGL
CFLAGS=-g -fopenmp -Wall # `pkg-config --cflags fann` # `pkg-config --cflags libjpeg`
ALL=create hEather xor

all: ${ALL}

fann0: fann0.o jpegdec.o comms.o
	gcc -o fann0 fann0.o jpegdec.o comms.o ${LDFLAGS}

fann1: fann1.o jpegdec.o comms.o
	gcc -o fann1 fann1.o jpegdec.o comms.o ${LDFLAGS}

create: create.o ann.o
	gcc -o create create.o ann.o -g -fopenmp -Wall -lm

hEather: hEather.o
	gcc -o hEather hEather.o -lm ${LDFLAGS}

xor: xor.o ann.o
	gcc -o xor xor.o ann.o -lm -g -fopenmp -Wall

test: test.o conv.o
	gcc -o test test.o conv.o ${LDFLAGS}

clean:
	rm -f *.o ${ALL}


