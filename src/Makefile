MKGTK?=		yes
_GTK_SH=	case "${MKGTK}" in \
		[Yy][Ee][Ss]) echo "dhcpcd-gtk";; \
		*) echo "";; \
		esac
_GTK!=		${_GTK_SH}
GTK=		${_GTK}$(shell ${_GTK_SH})

SUBDIR=		libdhcpcd ${GTK}

MK=../mk
include ${MK}/subdir.mk
