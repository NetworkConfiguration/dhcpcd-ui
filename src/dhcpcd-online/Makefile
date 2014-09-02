PROG=		dhcpcd-online
SRCS=		dhcpcd-online.c

MAN8=		dhcpcd-online.8

TOPDIR=		../..
include ${TOPDIR}/iconfig.mk

MAN8=		dhcpcd-online.8

CPPFLAGS+=	-I${TOPDIR}

LDADD+=		${LIB_DHCPCD} ${LIB_INTL}

include ../libdhcpcd/Makefile.inc
include ${MKDIR}/prog.mk
