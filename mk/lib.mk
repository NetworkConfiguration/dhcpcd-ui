# rules to build a library

SHLIB=			lib${LIB}.so.${SHLIB_MAJOR}
SHLIB_LINK=		lib${LIB}.so
LIBNAME=		lib${LIB}.a
SONAME?=		${SHLIB}

OBJS+=			${SRCS:.c=.o}
SOBJS+=			${OBJS:.o=.So}
LIBS?=			${LIBNAME} ${SHLIB}

CLEANFILES+=		${OBJS} ${SOBJS} ${LIBS} ${SHLIB_LINK}

LIBINSTALL?=		_libinstall

.SUFFIXES:		.So

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

.c.So:
	${CC} ${PICFLAG} -DPIC ${CPPFLAGS} ${CFLAGS} -c $< -o $@

all: depend ${LIBS}

${LIBNAME}:	${OBJS} ${STATICOBJS}
	@${ECHO} building static library $@
	${AR} cr $@ ${STATICOBJS} ${OBJS}
	${RANLIB} $@

${SHLIB}:	${SOBJS}
	@${ECHO} building shared library $@
	@rm -f $@ ${SHLIB_LINK}
	@ln -fs $@ ${SHLIB_LINK}
	${CC} ${LDFLAGS} -shared -Wl,-x -o $@ -Wl,-soname,${SONAME} \
		${SOBJS} ${LDADD}

_libinstall:	all
	${INSTALL} -d ${DESTDIR}${LIBDIR}
	${INSTALL} -m ${LIBMODE} ${LIBNAME} ${DESTDIR}${LIBDIR}
	${INSTALL} -d ${DESTDIR}${SHLIBDIR}
	${INSTALL} -m ${LIBMODE} ${SHLIB} ${DESTDIR}${SHLIBDIR}
	ln -fs ${SHLIB} ${DESTDIR}${SHLIBDIR}/${SHLIB_LINK}
	${INSTALL} -d ${DESTDIR}${INCDIR}
	for x in ${INCS}; do ${INSTALL} -m ${INCMODE} $$x ${DESTDIR}${INCDIR}; done

install: ${LIBINSTALL}

proginstall: install

clean:
	rm -f ${OBJS} ${SOBJS} ${LIBS} ${SHLIB_LINK} ${CLEANFILES}

extra_depend:
	@TMP=depend.$$$$; \
	${SED} -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.So:/' .depend > $${TMP}; \
	mv $${TMP} .depend

include ${MKDIR}/sys.mk
include ${MKDIR}/depend.mk
