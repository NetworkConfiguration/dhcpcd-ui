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

#ifndef DHCPCD_PREFERENCES_H
#define DHCPCD_PREFERENCES_H

#include <QDialog>

#include "dhcpcd.h"

class DhcpcdQt;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class DhcpcdPreferences : public QDialog
{
	Q_OBJECT

public:
	DhcpcdPreferences(DhcpcdQt *parent = 0);
	~DhcpcdPreferences();

protected:
	void closeEvent(QCloseEvent *e);

private slots:
	void clearConfig();
	void showConfig();
	void listBlocks(const QString &txt);
	void showBlock(const QString &txt);
	void rebind();
	void tryClose();

private:
	DhcpcdQt *parent;
	QComboBox *what;
	QComboBox *blocks;
	char *eBlock;
	char *eWhat;

	DHCPCD_IF *iface;
	char *name;
	int configIndex;
	DHCPCD_OPTION *config;
	const char *getString(QLineEdit *le);
	bool setOption(const char *opt, const char *val, bool *ret);
	bool makeConfig();
	bool changedConfig();
	bool writeConfig(bool *cancel);
	bool tryRebind(const char *ifname);

	QCheckBox *autoConf;
	QWidget *ipSetup;
	QLineEdit *ip;
	QLineEdit *router;
	QLineEdit *rdnss;
	QLineEdit *dnssl;

	QPushButton *clear;
};

#endif
