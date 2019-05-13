#
# Makefile for VSM++
#

DEMO_CODE=demo_code

TARGET=locknload
OBJ=locknload.o
HDR=
CFLAGS=-ggdb
.PHONY = clean all

all: ${TARGET} ${DEMO_CODE}

${TARGET}: ${OBJ}
	${CC} --static ${CFLAGS} $^ -o ${TARGET}

${DEMO_CODE}: ${DEMO_CODE}.o
	${CC} --static ${CFLAGS} $^ -o ${DEMO_CODE}
	@echo "Demo code built. Now create a locker binary from it."
	@echo
	@echo "  mkdir fs"
	@echo "  cp README.md fs"
	@echo "  ./mklocker.sh demo_code demo_code.lckr fs"
	@echo "  mv ./demo_code.lckr /tmp"
	@echo "  cd /tmp"
	@echo "  sudo ./demo_code.lckr"
	@echo

${OBJ}: ${HDR}


clean:
	rm -f ${TARGET} ${OBJ} *~ ${DEMO_CODE} ${DEMO_CODE}.o
