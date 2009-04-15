# Makefile based on BSD make.
# Our mk stubs also work with GNU make.
# Copyright 2008 Roy Marples <roy@marples.name>

PROG=		dhcpcd-gtk
SRCS=		main.c menu.c prefs.c wpa.c

ICONDIR?=	${PREFIX}/share/dhcpcd/icons
SYSCONFDIR?=	${PREFIX}/etc/xdg/autostart
FILESDIR?=	${SYSCONFDIR}
FILES=		dhcpcd-gtk.desktop

include ../libdhcpcd/Makefile.inc

CPPFLAGS+=	-DICONDIR=\"${ICONDIR}\"
CPPFLAGS+=	-DNOTIFY

_PKGCFLAGS_SH=	pkg-config --cflags gtk+-2.0 libnotify
_PKGCFLAGS!=	${_PKGCFLAGS_SH}
PKGCFLAGS?=	${_PKGCFLAGS}$(shell ${_PKGCFLAGS_SH})
CFLAGS+=	${PKGCFLAGS}

_PKGLIBS_SH=	pkg-config --libs gtk+-2.0 libnotify
_PKGLIBS!=	${_PKGLIBS_SH}
PKGLIBS?=	${_PKGLIBS}$(shell ${_PKGLIBS_SH})
LDADD+=		${PKGLIBS}

MK=		../../mk
include ${MK}/sys.mk
include ${MK}/prog.mk
