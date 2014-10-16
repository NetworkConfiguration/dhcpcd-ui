PROG=		dhcpcd-ui
VERSION=	0.7.4

TOPDIR=		.
include ${TOPDIR}/iconfig.mk
include ${MKDIR}/subdir.mk

.PHONY:		icons

SUBDIR=		src ${MKICONS}

FOSSILID?=	current
DISTPREFIX?=	${PROG}-${VERSION}
DISTFILEGZ?=	${DISTPREFIX}.tar.gz
DISTFILE?=	${DISTPREFIX}.tar.bz2

CLEANFILES+=	*.tar.bz2

_SNAP_SH=	date -u +%Y%m%d%H%M
_SNAP!=		${_SNAP_SH}
SNAP=		${_SNAP}$(shell ${_SNAP_SH})
SNAPDIR=	${DISTPREFIX}-${SNAP}
SNAPFILE=	${SNAPDIR}.tar.bz2

dist:
	fossil tarball --name ${DISTPREFIX} ${FOSSILID} /tmp/${DISTFILEGZ}
	rm -rf /tmp/${DISTPREFIX}
	tar -xzpf /tmp/${DISTFILEGZ} -C /tmp
	(cd /tmp/${DISTPREFIX}; make icons)
	tar -cvjpf ${DISTFILE} -C /tmp ${DISTPREFIX}
	rm -rf /tmp/${DISTPREFIX} /tmp/${DISTFILEGZ}
	ls -l ${DISTFILE}

distclean:
	(cd src; make clean)
	rm -f config.h config.mk config.log

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
