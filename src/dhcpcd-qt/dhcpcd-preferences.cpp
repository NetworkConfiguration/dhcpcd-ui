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
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <cerrno>

#include "config.h"
#include "dhcpcd-preferences.h"
#include "dhcpcd-ipv4validator.h"
#include "dhcpcd-qt.h"
#include "dhcpcd-wi.h"

DhcpcdPreferences::DhcpcdPreferences(DhcpcdQt *parent)
    : QDialog(parent)
{
	this->parent = parent;
	resize(400, 200);
	setWindowIcon(DhcpcdQt::getIcon("status", "network-transmit-receive"));
	setWindowTitle(tr("Network Preferences"));
	QPoint p = QCursor::pos();
	move(p.x(), p.y());

	name = NULL;
	config = NULL;
	configIndex = -1;
	eWhat = eBlock = NULL;
	iface = NULL;

	QVBoxLayout *layout = new QVBoxLayout();

	QGridLayout *topLayout = new QGridLayout();
	QLabel *conf = new QLabel(tr("Configure:"));
	topLayout->addWidget(conf, 0, 0);

	what = new QComboBox();
	connect(what, SIGNAL(currentIndexChanged(const QString &)),
	    this, SLOT(listBlocks(const QString &)));
	topLayout->addWidget(what, 0, 1);
	blocks = new QComboBox();
	connect(blocks, SIGNAL(currentIndexChanged(const QString &)),
	    this, SLOT(showBlock(const QString &)));
	topLayout->addWidget(blocks, 0, 2);

	topLayout->setColumnStretch(2, 10);

	QWidget *topBox = new QWidget();
	topBox->setLayout(topLayout);
	layout->addWidget(topBox);
	QFrame *topSep = new QFrame();
	topSep->setFrameShape(QFrame::HLine);
	topSep->setFrameShadow(QFrame::Sunken);
	layout->addWidget(topSep);

	autoConf = new QCheckBox(tr("Automatically configure empty options"));
	autoConf->setChecked(true);
	layout->addWidget(autoConf);

	DhcpcdIPv4Validator *v =
	    new DhcpcdIPv4Validator(DhcpcdIPv4Validator::Plain, this);
	DhcpcdIPv4Validator *vc =
	    new DhcpcdIPv4Validator(DhcpcdIPv4Validator::CIDR, this);
	DhcpcdIPv4Validator *vs =
	    new DhcpcdIPv4Validator(DhcpcdIPv4Validator::Spaced, this);
	ip = new QLineEdit();
	ip->setValidator(vc);
	router = new QLineEdit();
	router->setValidator(v);
	rdnss = new QLineEdit();
	rdnss->setValidator(vs);
	dnssl = new QLineEdit();
#if defined(__NetBSD__) || (__OpenBSD__)
	dnssl->setMaxLength(1024);
#else
	dnssl->setMaxLength(256);
#endif
	QFormLayout *ipLayout = new QFormLayout();
	ipLayout->addRow(tr("IP Address:"), ip);
	ipLayout->addRow(tr("Router:"), router);
	ipLayout->addRow(tr("DNS Servers:"), rdnss);
	ipLayout->addRow(tr("DNS Search:"), dnssl);
	ipSetup = new QWidget();
	ipSetup->setLayout(ipLayout);
	layout->addWidget(ipSetup);

	QHBoxLayout *buttonLayout = new QHBoxLayout();
	clear = new QPushButton(tr("&Clear"));
	clear->setIcon(QIcon::fromTheme("edit-clear"));
	buttonLayout->addWidget(clear);
	QPushButton *rebind = new QPushButton(tr("&Rebind"));
	rebind->setIcon(QIcon::fromTheme("application-x-executable"));
	buttonLayout->addWidget(rebind);
	QPushButton *close = new QPushButton(tr("&Close"));
	close->setIcon(QIcon::fromTheme("window-close"));
	buttonLayout->addWidget(close);
	QWidget *buttons = new QWidget();
	buttons->setLayout(buttonLayout);
	layout->addWidget(buttons);

	QIcon wired = DhcpcdQt::getIcon("devices", "network-wired");
	what->addItem(wired, tr("interface"));
	QIcon wireless = DhcpcdQt::getIcon("devices", "network-wireless");
	what->addItem(wireless, tr("SSID"));

	connect(clear, SIGNAL(clicked()), this, SLOT(clearConfig()));
	connect(rebind, SIGNAL(clicked()), this, SLOT(rebind()));
	connect(close, SIGNAL(clicked()), this, SLOT(tryClose()));

	setLayout(layout);

	autoConf->setEnabled(false);
	ipSetup->setEnabled(false);
	clear->setEnabled(false);

	DHCPCD_CONNECTION *con = parent->getConnection();
	if (!dhcpcd_config_writeable(con))
		QMessageBox::warning(this, tr("Not writeable"),
		    tr("The dhcpcd configuration file is not writeable\n\n%1")
		    .arg(dhcpcd_cffile(con)));
}

DhcpcdPreferences::~DhcpcdPreferences()
{

	free(eWhat);
	eWhat = NULL;
	free(eBlock);
	eBlock = NULL;
}

void DhcpcdPreferences::closeEvent(QCloseEvent *e)
{

	parent->dialogClosed(this);
	QDialog::closeEvent(e);
}

void DhcpcdPreferences::listBlocks(const QString &txt)
{
	char **list, **lp;
	QIcon icon;

	/* clear and then disconnect so we trigger a save */
	blocks->clear();
	blocks->disconnect(this);

	free(eWhat);
	eWhat = strdup(txt.toLower().toAscii());

	list = dhcpcd_config_blocks(parent->getConnection(),
	    txt.toLower().toAscii());

	if (txt == "interface") {
		char **ifaces, **i;

		blocks->addItem(tr("Select an interface"));
		ifaces = dhcpcd_interface_names_sorted(parent->getConnection());
		for (i = ifaces; i && *i; i++) {
			for (lp = list; lp && *lp; lp++) {
				if (strcmp(*i, *lp) == 0)
					break;
			}
			icon = DhcpcdQt::getIcon("actions",
			    lp && *lp ?
			    "document-save" : "document-new");
			blocks->addItem(icon, *i);
		}
		dhcpcd_freev(ifaces);
	} else {
		QList<DhcpcdWi *> *wis = parent->getWis();

		blocks->addItem(tr("Select a SSID"));
		for (int i = 0; i < wis->size(); i++) {
			DHCPCD_WI_SCAN *scan;
			DhcpcdWi *wi = wis->at(i);

			for (scan = wi->getScans(); scan; scan = scan->next) {
				for (lp = list; lp && *lp; lp++) {
					if (strcmp(scan->ssid, *lp) == 0)
						break;
				}
				icon = DhcpcdQt::getIcon("actions",
				    lp && *lp ?
				    "document-save" : "document-new");
				blocks->addItem(icon, scan->ssid);
			}
		}
	}

	dhcpcd_freev(list);

	/* Now make the 1st item unselectable and reconnect */
	qobject_cast<QStandardItemModel *>
	    (blocks->model())->item(0)->setEnabled(false);
	connect(blocks, SIGNAL(currentIndexChanged(const QString &)),
	    this, SLOT(showBlock(const QString &)));

}

void DhcpcdPreferences::clearConfig()
{
	QIcon icon = DhcpcdQt::getIcon("actions", "document-new");

	blocks->setItemIcon(blocks->currentIndex(), icon);
	autoConf->setChecked(true);
	ip->setText("");
	router->setText("");
	rdnss->setText("");
	dnssl->setText("");
}

void DhcpcdPreferences::showConfig()
{
	const char *val;
	bool a;

	if ((val = dhcpcd_config_get_static(config, "ip_address=")) != NULL)
                a = false;
        else
                a = !((val = dhcpcd_config_get(config, "inform")) == NULL &&
                    (iface && iface->flags & IFF_POINTOPOINT));
	autoConf->setChecked(a);
	ip->setText(val);
	router->setText(dhcpcd_config_get_static(config, "routers="));
	rdnss->setText(dhcpcd_config_get_static(config,"domain_name_servers="));
	dnssl->setText(dhcpcd_config_get_static(config, "domain_search="));
}

bool DhcpcdPreferences::changedConfig()
{
	const char *val;
	bool a;

	if ((val = dhcpcd_config_get_static(config, "ip_address=")) != NULL)
                a = false;
        else
                a = !((val = dhcpcd_config_get(config, "inform")) == NULL &&
                    (iface && iface->flags & IFF_POINTOPOINT));
	if (autoConf->isChecked() != a)
		return true;
	if (ip->text().compare(val))
		return true;
	val = dhcpcd_config_get_static(config, "routers=");
	if (router->text().compare(val))
		return true;
	val = dhcpcd_config_get_static(config, "domain_name_servers=");
	if (rdnss->text().compare(val))
		return true;
	val = dhcpcd_config_get_static(config, "domain_search=");
	if (rdnss->text().compare(val))
		return true;
	return false;
}


const char *DhcpcdPreferences::getString(QLineEdit *le)
{
	if (le->text().isEmpty())
		return NULL;
	return le->text().trimmed().toAscii();
}

bool DhcpcdPreferences::setOption(const char *opt, const char *val, bool *ret)
{
        if (opt[strlen(opt) - 1] == '=') {
                if (!dhcpcd_config_set_static(&config, opt, val))
                        qCritical("dhcpcd_config_set_static: %s",
                            strerror(errno));
                else
                        return true;
        } else {
                if (!dhcpcd_config_set(&config, opt, val))
                        qCritical("dhcpcd_config_set: %s",
                            strerror(errno));
                else
                        return true;
        }

        if (ret)
                *ret = false;
        return false;
}


bool DhcpcdPreferences::makeConfig()
{
	const char ns[] = "", *val;
	bool a, ret;

	a = autoConf->isChecked();
	ret = true;
	if (iface && iface->flags & IFF_POINTOPOINT)
		setOption("ip_address=", a ? NULL : ns, &ret);
        else {
		val = getString(ip);
                setOption("inform", a ? val : NULL, &ret);
                setOption("ip_address=", a ? NULL : val, &ret);
        }

        val = getString(router);
        setOption("routers=", val, &ret);

	val = getString(rdnss);
        setOption("domain_name_servers=", val, &ret);

	val = getString(dnssl);
        setOption("domain_search=", val, &ret);

        return ret;
}

bool DhcpcdPreferences::writeConfig(bool *cancel)
{

	switch (QMessageBox::question(this, tr("Save Configuration"),
	    tr("Do you want to save your changes?"),
	    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel)) {
	case QMessageBox::Cancel:
		*cancel = true;
		return false;
	case QMessageBox::Discard:
		*cancel = false;
		return true;
	default:
		break;
	}
	*cancel = false;

	DHCPCD_CONNECTION *con = parent->getConnection();
	if (!makeConfig()) {
		qCritical("failed to make config");
		goto err;
	}
	if (!dhcpcd_config_write(con, eWhat, eBlock, config)) {
		qCritical("dhcpcd_config_write: %s", strerror(errno));
		QMessageBox::critical(parent,
		    tr("Failed to write configuration"),
		    tr("Failed to write configuration:\n\n%1: %2")
		    .arg(dhcpcd_cffile(con))
		    .arg(strerror(errno)));
		goto err;
	}

	/* Braces to avoid jump error */
	{
		QIcon icon = DhcpcdQt::getIcon("actions", "document-save");
		blocks->setItemIcon(configIndex, icon);
	}
	return true;

err:
	/* Reload our config if there is a problem */
	config = dhcpcd_config_read(con, eWhat, eBlock);
	return false;
}

void DhcpcdPreferences::showBlock(const QString &txt)
{

	if (eBlock) {
		if (changedConfig()) {
			bool cancel;
			if (!writeConfig(&cancel))
				return;
		}
		free(eBlock);
	}
	if (txt.isEmpty())
		eBlock = NULL;
	else
		eBlock = strdup(txt.toAscii());

	dhcpcd_config_free(config);
	iface = NULL;
	DHCPCD_CONNECTION *con = parent->getConnection();

	if (eBlock && eWhat) {
		if (strcmp(eWhat, "interface") == 0)
			iface = dhcpcd_get_if(con, eBlock, "link");
		ip->setEnabled(iface == NULL ||
		    !(iface->flags & IFF_POINTOPOINT));
		errno = 0;
		config = dhcpcd_config_read(con, eWhat, eBlock);
		if (config == NULL && errno) {
			const char *s;

			s = strerror(errno);
			qCritical("dhcpcd_config_read: %s", s);
			QMessageBox::critical(this,
			    tr("Error reading configuration"),
			    tr("Error reading: ") + dhcpcd_cffile(con) +
			    "\n\n" + s);
		}
	} else
		config = NULL;

	if (config == NULL)
		configIndex = -1;
	else
		configIndex = blocks->currentIndex();

	showConfig();
	bool enabled = dhcpcd_config_writeable(con) && eBlock != NULL;
	autoConf->setEnabled(enabled);
	ipSetup->setEnabled(enabled);
	clear->setEnabled(enabled);
}

bool DhcpcdPreferences::tryRebind(const char *ifname)
{

	if (dhcpcd_rebind(parent->getConnection(), ifname) == 0)
		return true;

	qCritical("dhcpcd_rebind: %s", strerror(errno));
	QMessageBox::critical(this,
	    tr("Rebind failed"),
	    ifname ? tr("Failed to rebind interface %1: %2")
	    .arg(ifname).arg(strerror(errno)) :
	    tr("Failed to rebind: %1")
	    .arg(strerror(errno)));
	return false;
}

void DhcpcdPreferences::rebind()
{

	if (changedConfig()) {
		bool cancel;
		writeConfig(&cancel);
		if (cancel)
			return;
	}

	DHCPCD_CONNECTION *con = parent->getConnection();
	DHCPCD_IF *i;
	bool worked;
	bool found;
	if (eBlock == NULL || strcmp(eWhat, "interface") == 0) {
		worked = tryRebind(iface ? iface->ifname : NULL);
		goto done;
	}

	found = false;
	worked = true;
	for (i = dhcpcd_interfaces(con); i; i = i->next) {
		if (strcmp(i->type, "link") == 0 &&
		    (i->ssid && strcmp(i->ssid, eBlock) == 0))
		{
			found = true;
			if (!tryRebind(i->ifname))
				worked = false;
		}
	}
	if (!found) {
		QMessageBox::information(this,
		    tr("No matching interface"),
		    tr("No interface is bound to this SSID to rebind"));
		return;
	}

done:
	if (worked)
		close();
}

void DhcpcdPreferences::tryClose()
{

	if (changedConfig()) {
		bool cancel;
		writeConfig(&cancel);
		if (cancel)
			return;
	}
	close();
}
