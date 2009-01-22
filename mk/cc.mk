# Copyright 2008 Roy Marples <roy@marples.name>

# Setup some good default CFLAGS
CFLAGS?=	-O2

# Try and use some good cc flags if we're building from svn
# We don't use -pedantic as it will warn about our perfectly valid
# use of %m in our logger.
_CCFLAGS=	-Wall -Wextra -Wimplicit -Wshadow -Wformat=2 \
		-Wmissing-prototypes -Wmissing-declarations \
		-Wmissing-noreturn -Wmissing-format-attribute \
		-Wredundant-decls  -Wnested-externs \
		-Winline -Wwrite-strings -Wcast-align -Wcast-qual \
		-Wpointer-arith \
		-Wdeclaration-after-statement -Wsequence-point
_CC_FLAGS_SH=	if ! test -d .svn; then echo ""; else for f in ${_CCFLAGS}; do \
		if echo "int main(void) { return 0;} " | \
		${CC} $$f -S -xc -o /dev/null - ; \
		then printf "%s" "$$f "; fi \
		done; fi
_CC_FLAGS!=	${_CC_FLAGS_SH}
CFLAGS+=	${_CC_FLAGS}$(shell ${_CC_FLAGS_SH})
