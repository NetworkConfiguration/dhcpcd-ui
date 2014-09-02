LIB=		dhcpcd
SHLIB_MAJOR=	1
SRCS=		dhcpcd.c config.c wpa.c
INCS=		dhcpcd.h

TOPDIR=		../..
include ${TOPDIR}/iconfig.mk

CPPFLAGS+=	-I${TOPDIR}
LDADD+=		${LIB_INTL}

# Don't install
LIBINSTALL=	${LIB_DHCPCD_INSTALL}

include ${MKDIR}/lib.mk
