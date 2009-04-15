# Simple defaults

PREFIX?=	/usr/local

BINDIR?=	${PREFIX}/bin
BINMODE?=	0755
NONBINMODE?=	0644

SYSCONFDIR?=	${PREFIX}/etc

AR?=		ar
ECHO?=		echo
INSTALL?=	install
RANLIB?=	ranlib
SED?=		sed

_LIBNAME_SH=		case `readlink /lib` in "") echo "lib";; *) basename `readlink /lib`;; esac
_LIBNAME!=		${_LIBNAME_SH}
LIBNAME?=		${_LIBNAME}$(shell ${_LIBNAME_SH})
