LIB=		dhcpcd
SHLIB_MAJOR=	1
SRCS=		main.c config.c dispatch.c misc.c wpa.c

_PKGCFLAGS_SH=	pkg-config --cflags dbus-1
_PKGCFLAGS!=	${_PKGCFLAGS_SH}
PKGCFLAGS?=	${_PKGCFLAGS}$(shell ${_PKGCFLAGS_SH})
CFLAGS+=	${PKGCFLAGS}

_PKGLIBS_SH=	pkg-config --libs dbus-1
_PKGLIBS!=	${_PKGLIBS_SH}
PKGLIBS?=	${_PKGLIBS}$(shell ${_PKGLIBS_SH})
LDADD+=		${PKGLIBS}

# Don't install
LIBINSTALL=

MK=		../../mk
include ${MK}/lib.mk
include ${MK}/cc.mk
include ${MK}/debug.mk
