include ${MKDIR}/sys.mk

SIZEDIR?=	${SIZE}x${SIZE}
ICONDIR?=	${PREFIX}/share/dhcpcd/icons
IDIR=		${ICONDIR}/hicolor/${SIZEDIR}/${CATEGORY}

RSVG_CONVERT?=	rsvg-convert

ICONS+=		${SRCS:.svg=.png}
CLEANFILES+=	${SRCS:.svg=.png}

.SUFFIXES: .svg .png

all: ${ICONS}

.svg.png:
	${RSVG_CONVERT} -h ${SIZE} -w ${SIZE} $< >$@

_iconinstall: ${ICONS}
	${INSTALL} -d ${DESTDIR}${IDIR}
	${INSTALL} -m ${NONBINMODE} ${ICONS} ${DESTDIR}${IDIR}

proginstall:

install: _iconinstall

clean:
	rm -f ${CLEANFILES}
