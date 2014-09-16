# rules to build a program 
# based on FreeBSD's bsd.prog.mk

# Copyright 2008-2010 Roy Marples <roy@marples.name>

SRCS?=		${PROG}.c
OBJS+=		${SRCS:.c=.o}

all: ${PROG} ${SCRIPTS} ${FILES}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

${PROG}: .depend ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

proginstall: ${PROG}
	${INSTALL} -d ${DESTDIR}${BINDIR}
	${INSTALL} -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}

include ${MKDIR}/sys.mk
include ${MKDIR}/depend.mk
include ${MKDIR}/man.mk

install: proginstall ${FILESINSTALL} _maninstall

clean:
	rm -f ${OBJS} ${PROG} ${PROG}.core ${CLEANFILES}
