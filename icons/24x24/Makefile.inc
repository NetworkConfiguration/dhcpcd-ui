SRCDIR=		../../scalable

# .PATH is for BSD make, VPATH is for GNU make
.PATH:		${SRCDIR}/${CATEGORY}
VPATH+=		${SRCDIR}/${CATEGORY}

SIZE=		24

TOPDIR=         ../../..
include ${TOPDIR}/iconfig.mk
include ${MKDIR}/icon.mk
