LIB=		dhcpcd
SHLIB_MAJOR=	1
SRCS=		main.c config.c dispatch.c misc.c wpa.c
INCS=		libdhcpcd.h

TOPDIR=		../..
include ${TOPDIR}/iconfig.mk

CPPFLAGS+=	-I${TOPDIR} ${DBUS_CFLAGS}
CFLAGS+=	${DBUS_CFLAGS}
LDADD+=		${DBUS_LIBS}

# Don't install
LIBINSTALL=	${LIB_DHCPCD_INSTALL}

include ${MKDIR}/lib.mk
