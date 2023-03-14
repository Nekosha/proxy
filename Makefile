include config.mk

SRC = proxy.c varnum.c debug.c
OBJ = ${SRC:.c=.o}

all: proxy

.c.o:
	${CC} -c ${CFLAGS} $<

config.h: config.def
	cp config.def config.h
${OBJ}: config.mk config.h

proxy: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS} ${LIBS}
clean:
	rm -f proxy ${OBJ}

.PHONY: all clean proxy
