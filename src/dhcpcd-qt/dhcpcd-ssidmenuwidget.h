/*
 * dhcpcd-qt
 * Copyright 2014 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <QFrame>

#include "dhcpcd.h"

class QLabel;

class DhcpcdWi;

class DhcpcdSsidMenuWidget : public QFrame
{
	Q_OBJECT

public:
	DhcpcdSsidMenuWidget(QWidget *parent,
	    DhcpcdWi *wi, DHCPCD_WI_SCAN *scan);
	~DhcpcdSsidMenuWidget() {};

	DHCPCD_WI_SCAN *getScan();
	void setScan(DHCPCD_WI_SCAN *scan);
	bool isAssociated();

signals:
	void triggered();
	void hovered();

private slots:
	bool eventFilter(QObject *obj, QEvent *event);

protected slots:
	virtual void enterEvent(QEvent *event);
	virtual void leaveEvent(QEvent *event);

private:
	DhcpcdWi *wi;
	DHCPCD_WI_SCAN *scan;
	bool associated;

	QLabel *selicon;
	QLabel *ssid;
	QLabel *encicon;
	QLabel *stricon;
};
