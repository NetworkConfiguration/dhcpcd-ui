PROG=		dhcpcd-ui
VERSION=	0.7.2

TOPDIR=		.
include ${TOPDIR}/iconfig.mk
include ${MKDIR}/subdir.mk

.PHONY:		icons

SUBDIR=		src ${MKICONS}

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
	mkdir /tmp/${DISTPREFIX}
	cp -RPp * /tmp/${DISTPREFIX}
	(cd /tmp/${DISTPREFIX}; \
		./configure; make clean icons; rm config.h config.mk)
	find /tmp/${DISTPREFIX} -name .gitignore -delete
	tar -cvjpf ${DISTFILE} -C /tmp ${DISTPREFIX}
	rm -rf /tmp/${DISTPREFIX}
	ls -l ${DISTFILE}

distclean:
	(cd src; make clean)
	rm -f config.h config.mk

snapshot: icons
	mkdir /tmp/${SNAPDIR}
	cp -RPp * /tmp/${SNAPDIR}
	(cd /tmp/${SNAPDIR}; make clean; rm config.h config.mk)
	find /tmp/${SNAPDIR} -name .gitignore -delete
	tar -cvjpf ${SNAPFILE} -C /tmp ${SNAPDIR}
	rm -rf /tmp/${SNAPDIR}
	ls -l ${SNAPFILE}

snap: snapshot

icons:
	${MAKE} -C icons
