# rules to build a program 
# based on FreeBSD's bsd.prog.mk

# Copyright 2008-2010 Roy Marples <roy@marples.name>

SRCS?=		${PROG}.c
OBJS+=		${SRCS:.c=.o}

all: ${PROG} ${SCRIPTS}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

${PROG}: .depend ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

_proginstall: ${PROG}
	${INSTALL} -d ${DESTDIR}${BINDIR}
	${INSTALL} -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}

include ${MKDIR}/sys.mk
include ${MKDIR}/depend.mk
include ${MKDIR}/files.mk

install: _proginstall _filesinstall

clean:
	rm -f ${OBJS} ${PROG} ${PROG}.core ${CLEANFILES}
