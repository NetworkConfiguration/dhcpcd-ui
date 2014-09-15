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

#ifndef DHCPCD_IPV4VALIDATOR_H
#define DHCPCD_IPV4VALIDATOR_H

#include <QValidator>

class DhcpcdIPv4Validator : public QValidator
{
	Q_OBJECT

public:
	enum Flag {
		Plain = 0x0,
		CIDR = 0x01,
		Spaced = 0x02
	};
	Q_DECLARE_FLAGS(Flags, Flag)
	explicit DhcpcdIPv4Validator(DhcpcdIPv4Validator::Flags flag = Plain, QObject *parent = 0);
	QValidator::State validate(QString &input, int &pos) const;

private:
	DhcpcdIPv4Validator::Flags flags;
	QValidator::State validate1(QString &input) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DhcpcdIPv4Validator::Flags)
#endif
