CONFIG+=		qt gui c++11 debug
QMAKE_CXXFLAGS+=	-std=c++11 -O2

HEADERS=		dhcpcd-qt.h dhcpcd-about.h dhcpcd-preferences.h \
			dhcpcd-wi.h dhcpcd-ifmenu.h dhcpcd-ssidmenu.h \
			dhcpcd-ipv4validator.h
SOURCES=		main.cpp dhcpcd-qt.cpp dhcpcd-about.cpp \
			dhcpcd-preferences.cpp dhcpcd-wi.cpp \
			dhcpcd-ifmenu.cpp dhcpcd-ssidmenu.cpp \
			dhcpcd-ipv4validator.cpp

INCLUDEPATH+=		../../
INCLUDEPATH+=		../libdhcpcd/

LIBS+=			-L../libdhcpcd ../libdhcpcd/libdhcpcd.a

has_libintl {
	LIBS +=		-lintl
}

QMAKE_CLEAN+=		${TARGET}
