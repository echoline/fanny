LDFLAGS=-g `pkg-config --libs libjpeg` `pkg-config --libs x11` -fopenmp
CFLAGS=-g `pkg-config --cflags libjpeg` `pkg-config --cflags x11` -fopenmp
ALL=hEather

all: ${ALL}

fann0: fann0.o jpegdec.o comms.o
	gcc -o fann0 fann0.o jpegdec.o comms.o ${LDFLAGS}

fann1: fann1.o jpegdec.o comms.o
	gcc -o fann1 fann1.o jpegdec.o comms.o ${LDFLAGS}

create: create.o
	gcc -o create create.o ${LDFLAGS}

hEather: hEather.o ann.o
	gcc -o hEather hEather.o -lX11 -lm ${LDFLAGS} ann.o

clean:
	rm -f *.o ${ALL}


