PROG=		dhcpcd-curses
SRCS=		dhcpcd-curses.c eloop.c

TOPDIR=		../..
include ${TOPDIR}/iconfig.mk

MAN8=		dhcpcd-curses.8

CPPFLAGS+=	-I${TOPDIR} ${CURSES_CPPFLAGS}
LDADD+=		${LIB_DHCPCD} ${LIB_CURSES} ${LIB_INTL}

.PHONY: dhcpcd-curses

include ../libdhcpcd/Makefile.inc

include ${MKDIR}/prog.mk
