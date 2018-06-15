LDFLAGS=-lfann -ljpeg -g
CFLAGS=-g
ALL=create hEather

all: ${ALL}

fann0: fann0.o jpegdec.o comms.o
	gcc -o fann0 fann0.o jpegdec.o comms.o ${LDFLAGS}

fann1: fann1.o jpegdec.o comms.o
	gcc -o fann1 fann1.o jpegdec.o comms.o ${LDFLAGS}

create: create.o
	gcc -o create create.o ${LDFLAGS}

hEather: hEather.c
	gcc -o hEather hEather.c -lX11 -lfftw3 -lm ${LDFLAGS}

clean:
	rm -f *.o ${ALL}

