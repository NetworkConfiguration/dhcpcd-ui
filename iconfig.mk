# Nasty hack so that make clean works without configure being run
# Requires gmake4
_CONFIG_MK!=	test -e ${TOPDIR}/config.mk && echo config.mk || echo config-null.mk
CONFIG_MK?=	${_CONFIG_MK}
TOP?=		.
include		${TOPDIR}/${CONFIG_MK}
