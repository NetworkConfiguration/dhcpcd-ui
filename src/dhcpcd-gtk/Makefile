PROG=		dhcpcd-gtk
SRCS=		main.c menu.c prefs.c wpa.c

TOPDIR=		../..
include ${TOPDIR}/iconfig.mk

ICONDIR?=	${PREFIX}/share/dhcpcd/icons
APPDIR?=	${PREFIX}/share/applications
FILESDIR=	${APPDIR}
FILES=		dhcpcd-gtk.desktop
MAN8=		dhcpcd-gtk.8

CPPFLAGS+=	${NOTIFY} -I${TOPDIR}

CFLAGS+=	${GTK_CFLAGS}
LDADD+=		${LIB_DHCPCD} ${GTK_LIBS}

.PHONY: dhcpcd-gtk

include ../libdhcpcd/Makefile.inc

FILESINSTALL=	_filesinstall
include ${MKDIR}/prog.mk
include ${MKDIR}/files.mk
