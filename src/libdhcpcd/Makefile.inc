CPPFLAGS+=	-I../libdhcpcd
LDFLAGS+=	-L../libdhcpcd

# We normally want to build in the static library
MKSTATIC?=	yes
_LDADD_SH=	case "${MKSTATIC}" in \
		[Yy][Ee][Ss]) echo "-Wl,-Bstatic -ldhcpcd -Wl,-Bdynamic";; \
		*) echo "-ldhcpcd";; \
		esac
_LDADD!=	${_LDADD_SH}
LDADD+=		${_LDADD}$(shell ${_LDADD_SH})

TARGETS+=	../libdhcpcd/libdhcpcd.a
