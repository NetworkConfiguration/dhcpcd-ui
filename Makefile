PROG=		dhcpcd-ui
VERSION=	0.4.2

.PHONY:		icons

SUBDIR=		src icons

MK=	mk
include ${MK}/subdir.mk
include ${MK}/dist.mk

icons:
	${MAKE} -C icons
