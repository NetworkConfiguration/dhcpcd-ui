# Makefile based on BSD make.
# Our mk stubs also work with GNU make.
# Copyright 2008 Roy Marples <roy@marples.name>

PROG=		dhcpcd-gtk
SRCS=		main.c menu.c

SYSCONFDIR?=	${PREFIX}/etc/xdg/autostart
FILESDIR?=	${SYSCONFDIR}
FILES=		dhcpcd-gtk.desktop

# Crappy include for Desktop Environment
# We have mk for GNOME and XFCE
include		de-${DE}.mk 

_PKGCFLAGS_SH=	pkg-config --cflags dbus-glib-1 gtk+-2.0 libnotify ${DEPKGS}
_PKGCFLAGS!=	${_PKGCFLAGS_SH}
PKGCFLAGS?=	${_PKGCFLAGS}$(shell ${_PKGCFLAGS_SH})
CFLAGS+=	${PKGCFLAGS}

_PKGLIBS_SH=	pkg-config --libs dbus-glib-1 gtk+-2.0 libnotify ${DEPKGS}
_PKGLIBS!=	${_PKGLIBS_SH}
PKGLIBS?=	${_PKGLIBS}$(shell ${_PKGLIBS_SH})
LDADD+=		${PKGLIBS}

CPPFLAGS+=	${DECPPFLAGS}

MK=		mk
include ${MK}/sys.mk
include ${MK}/prog.mk
