PROG=		dhcpcd-decode
SRCS=		dhcpcd-decode.c

MAN8=		dhcpcd-decode.8

TOPDIR=		../..
include ${TOPDIR}/iconfig.mk

CPPFLAGS+=	-I${TOPDIR}

LDADD+=		${LIB_DHCPCD} ${LIB_INTL}

include ../libdhcpcd/Makefile.inc
include ${MKDIR}/prog.mk
