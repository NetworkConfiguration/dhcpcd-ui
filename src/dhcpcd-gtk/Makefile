PROG=		dhcpcd-gtk
SRCS=		main.c menu.c prefs.c wpa.c

TOPDIR=		../..
include ${TOPDIR}/config.mk

ICONDIR?=	${PREFIX}/share/dhcpcd/icons
SYSCONFDIR?=	${PREFIX}/etc/xdg/autostart
FILESDIR?=	${SYSCONFDIR}
FILES=		dhcpcd-gtk.desktop

CPPFLAGS+=	${NOTIFY} -I${TOPDIR}

CFLAGS+=	${GTK_CFLAGS}
LDADD+=		${LIB_DHCPCD} ${GTK_LIBS}

include ../libdhcpcd/Makefile.inc
include ${MKDIR}/prog.mk
