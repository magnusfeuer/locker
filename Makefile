#
# Makefile for VSM++
#

TARGET=locker
OBJ=locker.o
HDR=
CFLAGS=-ggdb
.PHONY = clean all

all: ${TARGET}

${TARGET}: ${OBJ}
	${CC} ${CFLAGS} $^ -o ${TARGET}

${OBJ}: ${HDR}


clean:
	rm -f ${TARGET} ${OBJ} *~
