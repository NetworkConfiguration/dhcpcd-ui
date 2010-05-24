# Generate .depend
# Copyright 2008 Roy Marples <roy@marples.name>

CLEANFILES+=	.depend

.depend: ${SRCS}
	rm -f .depend
	${CC} ${CPPFLAGS} ${CFLAGS} -MM ${SRCS} > .depend

depend: .depend
