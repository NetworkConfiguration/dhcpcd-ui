# Simple defaults

PREFIX?=	/usr/local

BINDIR?=	${PREFIX}/bin
BINMODE?=	0755
NONBINMODE?=	0644
LIBMODE?=	${NONBINMODE}
MANMODE?=	${NONBINMODE}

INCDIR?=	${PREFIX}/include
INCMODE?=	${NONBINMODE}

SYSCONFDIR?=	${PREFIX}/etc

AR?=		ar
ECHO?=		echo
INSTALL?=	install
RANLIB?=	ranlib
SED?=		sed

PICFLAG?=	-fPIC

LIBNAME?=	lib
