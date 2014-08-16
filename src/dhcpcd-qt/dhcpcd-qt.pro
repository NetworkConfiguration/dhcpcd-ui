CONFIG+=		qt gui c++11 debug
QMAKE_CXXFLAGS+=	-std=c++11

HEADERS=		dhcpcd-qt.h dhcpcd-about.h dhcpcd-preferences.h \
			dhcpcd-wi.h dhcpcd-ssidmenu.h
SOURCES=		main.cpp dhcpcd-qt.cpp dhcpcd-about.cpp \
			dhcpcd-preferences.cpp dhcpcd-wi.cpp dhcpcd-ssidmenu.cpp

INCLUDEPATH+=		../../
INCLUDEPATH+=		../libdhcpcd/

LIBS+=			-L../libdhcpcd ../libdhcpcd/libdhcpcd.a

QMAKE_CLEAN+=		${TARGET}
