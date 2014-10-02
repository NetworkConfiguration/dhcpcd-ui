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

has_libkdeui {
	LIBS+=		-lkdeui
	DEFINES+=	NOTIFY
	INSTALLS+=	notifyrc
}

QMAKE_CLEAN+=		${TARGET}

isEmpty(PREFIX) {
	PREFIX=		/usr/local
}
isEmpty(SYSCONFDIR) {
	SYSCONFDIR=	$$PREFIX/etc
}
isEmpty(MANDIR) {
	MANDIR=		$$PREFIX/share/man
}

target.path=		$$PREFIX/bin

man8.path=		$$MANDIR/man8
man8.files=		dhcpcd-qt.8

desktop.path=		$$PREFIX/share/applications
desktop.files=		dhcpcd-qt.desktop

notifyrc.path=		$$PREFIX/share/apps/dhcpcd-qt
notifyrc.files=		dhcpcd-qt.notifyrc

INSTALLS+=		target man8 desktop
