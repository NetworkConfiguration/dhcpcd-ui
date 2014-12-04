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

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "dhcpcd.h"
#include "dhcpcd-qt.h"
#include "dhcpcd-ssid.h"
#include "dhcpcd-wi.h"

DhcpcdSsid::DhcpcdSsid(DhcpcdWi *parent, DHCPCD_WI_SCAN *scan)
    : QDialog()
{
	this->parent = parent;
	QVBoxLayout *layout;
	setWindowIcon(DhcpcdQt::getIcon("status",
	    "network-wireless-encrypted"));
	setWindowTitle(scan->ssid);
	setMinimumWidth(300);
	resize(10, 10);
	QPoint p = QCursor::pos();
	move(p.x(), p.y());

	layout = new QVBoxLayout(this);

	QHBoxLayout *hbox = new QHBoxLayout();
	layout->addLayout(hbox);
	QLabel *label = new QLabel(tr("Pre Shared Key:"));
	hbox->addWidget(label);
	psk = new QLineEdit();
	psk->setSizePolicy(QSizePolicy::Expanding,
	    QSizePolicy::Fixed);
	hbox->addWidget(psk);

	hbox = new QHBoxLayout();
	layout->addLayout(hbox);
	hbox->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
	    QSizePolicy::Expanding));
	QDialogButtonBox *okcancel = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	hbox->addWidget(okcancel);
	connect(okcancel, SIGNAL(accepted()), this, SLOT(accept()));
	connect(okcancel, SIGNAL(rejected()), this, SLOT(reject()));
}

QString DhcpcdSsid::getPsk(bool *ok)
{
	int r;

	exec();
	if (result() == QDialog::Rejected) {
		if (ok)
			*ok = false;
		return QString::Null();
	}

	if (ok)
		*ok = true;
	return psk->text();
}
