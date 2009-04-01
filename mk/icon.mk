include ${MK}/sys.mk

SIZEDIR?=	${SIZE}x${SIZE}
ICONDIR?=	${PREFIX}/share/dhcpcd/icons/hicolor/${SIZEDIR}/${CATEGORY}

RSVG?=		rsvg

ICONS+=		${SRCS:.svg=.png}
CLEANFILES+=	${SRCS:.svg=.png}

.SUFFIXES: .svg .png

all: ${ICONS}

.svg.png:
	${RSVG} -h ${SIZE} -w ${SIZE} $< $@

_iconinstall: ${ICONS}
	${INSTALL} -d ${DESTDIR}${ICONDIR}
	${INSTALL} -m ${BINMODE} ${ICONS} ${DESTDIR}${ICONDIR}

install: _iconinstall

clean:
	rm -f ${CLEANFILES}
