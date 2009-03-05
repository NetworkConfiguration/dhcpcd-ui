# Makefile based on BSD make.
# Our mk stubs also work with GNU make.
# Copyright 2008 Roy Marples <roy@marples.name>

PROG=		dhcpcd-gtk
SRCS=		main.c menu.c dhcpcd-config.c prefs.c wpa.c

SYSCONFDIR?=	${PREFIX}/etc/xdg/autostart
FILESDIR?=	${SYSCONFDIR}
FILES=		dhcpcd-gtk.desktop

_PKGCFLAGS_SH=	pkg-config --cflags dbus-glib-1 gtk+-2.0 libnotify
_PKGCFLAGS!=	${_PKGCFLAGS_SH}
PKGCFLAGS?=	${_PKGCFLAGS}$(shell ${_PKGCFLAGS_SH})
CFLAGS+=	${PKGCFLAGS}

_PKGLIBS_SH=	pkg-config --libs dbus-glib-1 gtk+-2.0 libnotify
_PKGLIBS!=	${_PKGLIBS_SH}
PKGLIBS?=	${_PKGLIBS}$(shell ${_PKGLIBS_SH})
LDADD+=		${PKGLIBS}

MK=		mk
include ${MK}/sys.mk
include ${MK}/prog.mk
