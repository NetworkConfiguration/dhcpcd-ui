# rules to make a distribution tarball from a svn repo
# Copyright 2008 Roy Marples <roy@marples.name>

GITREF?=	HEAD
DISTPREFIX?=	${PROG}-${VERSION}
DISTFILE?=	${DISTPREFIX}.tar.bz2

CLEANFILES+=	*.tar.bz2

_SNAP_SH=	date -u +%Y%m%d%H%M
_SNAP!=		${_SNAP_SH}
SNAP=		${_SNAP}$(shell ${_SNAP_SH})
SNAPDIR=	${DISTPREFIX}-${SNAP}
SNAPFILE=	${SNAPDIR}.tar.bz2

dist:
	git archive --prefix=${DISTPREFIX}/ ${GITREF} | bzip2 > ${DISTFILE}

snapshot:
	mkdir /tmp/${SNAPDIR}
	cp -RPp * /tmp/${SNAPDIR}
	(cd /tmp/${SNAPDIR}; make clean)
	tar -cvjpf ${SNAPFILE} -C /tmp ${SNAPDIR}
	rm -rf /tmp/${SNAPDIR}
	ls -l ${SNAPFILE}

snap: snapshot
