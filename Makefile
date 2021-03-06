LDFLAGS=-g `pkg-config --libs fann` `pkg-config --libs libjpeg` -fopenmp
CFLAGS=-g `pkg-config --cflags fann` `pkg-config --cflags libjpeg` -fopenmp
ALL=create hEather

all: ${ALL}

fann0: fann0.o jpegdec.o comms.o
	gcc -o fann0 fann0.o jpegdec.o comms.o ${LDFLAGS}

fann1: fann1.o jpegdec.o comms.o
	gcc -o fann1 fann1.o jpegdec.o comms.o ${LDFLAGS}

create: create.o
	gcc -o create create.o ${LDFLAGS}

hEather: hEather-simple.o
	gcc -o hEather hEather-simple.o -lm ${LDFLAGS}

test: test.o conv.o
	gcc -o test test.o conv.o ${LDFLAGS}

clean:
	rm -f *.o ${ALL}


