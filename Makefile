PROG=		dhcpcd-ui
VERSION=	0.7.5

TOPDIR=		.
include ${TOPDIR}/iconfig.mk
include ${MKDIR}/subdir.mk

.PHONY:		icons

SUBDIR=		src ${MKICONS}

FOSSILID?=	current
DISTPREFIX?=	${PROG}-${VERSION}
DISTFILEGZ?=	${DISTPREFIX}.tar.gz
DISTFILE?=	${DISTPREFIX}.tar.xz
DISTINFO=	${DISTFILE}.distinfo
DISTINFOSIGN=	${DISTINFO}.asc
CKSUM?=		cksum -a SHA256
PGP?=		netpgp

CLEANFILES+=	${DISTFILE} ${DISTINFO} ${DISTINFOSIGN}

_SNAP_SH=	date -u +%Y%m%d%H%M
_SNAP!=		${_SNAP_SH}
SNAP=		${_SNAP}$(shell ${_SNAP_SH})
SNAPDIR=	${DISTPREFIX}-${SNAP}
SNAPFILE=	${SNAPDIR}.tar.xz

dist:
	fossil tarball --name ${DISTPREFIX} ${FOSSILID} /tmp/${DISTFILEGZ}
	rm -rf /tmp/${DISTPREFIX}
	tar -xzpf /tmp/${DISTFILEGZ} -C /tmp
	(cd /tmp/${DISTPREFIX}; make icons)
	rm -rf /tmp/${DISTPREFIX}/doc
	tar -cvJpf ${DISTFILE} -C /tmp ${DISTPREFIX}
	rm -rf /tmp/${DISTPREFIX} /tmp/${DISTFILEGZ}

distinfo: dist
	${CKSUM} ${DISTFILE} >${DISTINFO}
	#printf "SIZE (${DISTFILE}) = %s\n" $$(wc -c <${DISTFILE}) >>${DISTINFO}
	${PGP} --sign --detach --armor --output=${DISTINFOSIGN} ${DISTINFO}
	chmod 644 ${DISTINFOSIGN}
	ls -l ${DISTFILE} ${DISTINFO} ${DISTINFOSIGN}

distclean:
	(cd src; make clean)
	rm -f config.h config.mk config.log

snapshot: icons
	mkdir /tmp/${SNAPDIR}
	cp -RPp * /tmp/${SNAPDIR}
	(cd /tmp/${SNAPDIR}; make clean; rm config.h config.mk)
	find /tmp/${SNAPDIR} -name .gitignore -delete
	tar -cvJpf ${SNAPFILE} -C /tmp ${SNAPDIR}
	rm -rf /tmp/${SNAPDIR}
	ls -l ${SNAPFILE}

snap: snapshot

icons:
	${MAKE} -C icons
