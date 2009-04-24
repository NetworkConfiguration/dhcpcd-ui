# Generate .depend
# Copyright 2008 Roy Marples <roy@marples.name>

CLEANFILES+=	.depend

.depend: ${SRCS}
	rm -f .depend
	${CC} ${CPPFLAGS} ${CFLAGS} -MM ${SRCS} > .depend

depend: .depend


# Nasty hack. depend-.mk is a blank file, depend-gmake.mk has a gmake specific
# command to optionally include .depend.
# Someone should patch gmake to optionally include .depend if it exists.
_INC_DEP=	$(shell if ${MAKE} --version | grep -q "^GNU "; then \
		echo "gmake"; else echo ""; fi)
include ${MK}/depend-${_INC_DEP}.mk
