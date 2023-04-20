PROG=		dhcpcd-ui
PACKAGE=	${PROG}
VERSION=	0.7.8

TOPDIR=		.
include ${TOPDIR}/iconfig.mk
include ${MKDIR}/subdir.mk

.PHONY:		icons

SUBDIR=		src ${MKICONS}

DIST!=		if test -d .git; then echo "dist-git"; \
		else echo "dist-inst"; fi
DISTPREFIX?=	${PACKAGE}-${VERSION}
DISTFILETAR?=	${DISTPREFIX}.tar
DISTFILE?=	${DISTFILETAR}.xz
DISTINFO=	${DISTFILE}.distinfo
DISTINFOMD=	${DISTINFO}.md
DISTSIGN=	${DISTFILE}.asc

CLEANFILES+=	${DISTFILE} ${DISTINFO} ${DISTINFOSIGN}

_SNAP_SH=	date -u +%Y%m%d%H%M
_SNAP!=		${_SNAP_SH}
SNAP=		${_SNAP}$(shell ${_SNAP_SH})
SNAPDIR=	${DISTPREFIX}-${SNAP}
SNAPFILE=	${SNAPDIR}.tar.xz

SHA256?=	sha256
PGP?=		gpg

dist-git:
	git archive --prefix=${DISTPREFIX}/ v${VERSION} | xz >${DISTFILE}

dist-inst:
	mkdir /tmp/${DISTPREFIX}
	cp -RPp * /tmp/${DISTPREFIX}
	(cd /tmp/${DISTPREFIX}; make clean)
	tar -cvJpf ${DISTFILE} -C /tmp ${DISTPREFIX}
	rm -rf /tmp/${DISTPREFIX}

dist: ${DIST}

distinfo: dist
	rm -f ${DISTINFO} ${DISTSIGN}
	${SHA256} ${DISTFILE} >${DISTINFO}
	${PGP} --armour --detach-sign ${DISTFILE}
	chmod 644 ${DISTSIGN}
	ls -l ${DISTFILE} ${DISTINFO} ${DISTSIGN}

${DISTINFOMD}: ${DISTINFO}
	echo '```' >${DISTINFOMD}
	cat ${DISTINFO} >>${DISTINFOMD}
	echo '```' >>${DISTINFOMD}

release: distinfo ${DISTINFOMD}
	gh release create v${VERSION} \
		--title "${PACKAGE} ${VERSION}" --draft --generate-notes \
		--notes-file ${DISTINFOMD} \
		${DISTFILE} ${DISTSIGN}

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
