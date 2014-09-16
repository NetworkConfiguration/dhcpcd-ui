# Quick and dirty files
# Copyright 2008 Roy Marples <roy@marples.name>

FILESDIR?=	${BINDIR}
FILESMODE?=	${NONBINMODE}

FILESINSTALL=	_filesinstall

_filesinstall:
	${INSTALL} -d ${DESTDIR}${FILESDIR}
	${INSTALL} -m ${FILESMODE} ${FILES} ${DESTDIR}${FILESDIR}
	${INSTALL} -d ${DESTDIR}${FILESDIR2}
	${INSTALL} -m ${FILESMODE} ${FILES2} ${DESTDIR}${FILESDIR2}

