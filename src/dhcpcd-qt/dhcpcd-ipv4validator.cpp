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

#include <QStringList>

#include "dhcpcd-ipv4validator.h"

DhcpcdIPv4Validator::DhcpcdIPv4Validator(Flags flags, QObject *parent)
{
	this->flags = flags;
	this->setParent(parent);
}

QValidator::State DhcpcdIPv4Validator::validate1(QString &input) const
{
	if (input.isEmpty())
		return Acceptable;

	QStringList slist = input.split('.');
	int sl = slist.size();
	if (sl > 4)
		return Invalid;

	bool ok, empty;
	int cidr, val;
	cidr = -1;
	empty = false;
	QValidator::State CIDRstate = Acceptable;
	for (int i = 0; i < sl; i++) {
		QString s = slist[i];
		if (i == sl - 1 && flags.testFlag(DhcpcdIPv4Validator::CIDR)) {
			QStringList ssplit = s.split('/');
			s = ssplit[0];
			val = ssplit.size();
			if (val == 2) {
				if (ssplit[1].isEmpty())
					CIDRstate = Intermediate;
				else {
					cidr = ssplit[1].toInt(&ok);
					if (!ok || cidr < 0 || cidr > 32)
						return Invalid;
				}
			} else if (val != 1)
				return Invalid;
		}
		if (s.isEmpty()) {
			if (empty)
				return Invalid;
			empty = true;
		} else {
			val = s.toInt(&ok);
			if (!ok || val < 0 || val > 255)
				return Invalid;
		}
	}
	if (sl < 4 && cidr == -1)
		return Intermediate;
	return CIDRstate;
}

QValidator::State DhcpcdIPv4Validator::validate(QString &input, int &) const
{
	if (input.isEmpty())
		return Acceptable;

	QStringList slist = input.split(' ');
	int sl  = slist.size();
	if (sl > 1 && !flags.testFlag(DhcpcdIPv4Validator::Spaced))
		return Invalid;

	QValidator::State state = Acceptable;
	for (int i = 0; i < sl; i++) {
		state = validate1(slist[i]);
		if (state == Invalid)
			return Invalid;
		if (state == Intermediate && i + 1 < sl)
			return Invalid;
	}
	return state;
}
