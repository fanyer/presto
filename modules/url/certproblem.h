/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2005-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef CERTPROBLEM_H
#define CERTPROBLEM_H

#include "modules/url/url_sn.h"

#include "modules/util/simset.h"


class CertInfo : public Link
{
public:
	CertInfo() : cert_id(0), cert_id_len(0), valid_from(0), valid_until(0) { }

	CertInfo *Suc() const { return (CertInfo *)Link::Suc(); }

	void SetCertId(char *id, int len) {
		OP_DELETEA(cert_id);
		cert_id = id;
		cert_id_len = len;
	}
	OP_STATUS SetIssuer(const uni_char *str) { return issuer.Set(str); }
	OP_STATUS SetSubject(const uni_char *str) { return subject.Set(str); }
	OP_STATUS SetCommonName(const uni_char *str) {
		return common_name.Set(str);
	}
	void SetValidityPeriod(time_t start, time_t end) {
		valid_from = start;
		valid_until = end;
	}

	const char *GetCertId() const { return cert_id; }
	int GetCertIdLen() const { return cert_id_len; }
	const uni_char *GetIssuer() const { return issuer.CStr(); }
	const uni_char *GetSubject() const { return subject.CStr(); }
	const uni_char *GetCommonName() const { return common_name.CStr(); }
	const time_t GetValidFrom() const { return valid_from; }
	const time_t GetValidUntil() const { return valid_until; }

private:
	OpString issuer;
	OpString subject;
	OpString common_name;
	char *cert_id;
	int cert_id_len;
	time_t valid_from;
	time_t valid_until;
};


class CertProblem : public Link
{
public:
	enum Type
	{
		UNKNOWN,
		NOT_TRUSTED,
		NOMATCH_FORMAT,
		INVALID_DATE,
		BASIC_CONSTRAINT,
		KEY_USAGE,
		SIGNATURE_INVALID,
		CHAIN
	};

	CertProblem(Type type, unsigned int id, ServerName *sname)
		: problemtype(type), reporter_id(id), servername(sname) { }
	~CertProblem() { certificates.Clear(); }

	CertProblem *Suc() const { return (CertProblem *)Link::Suc(); }

	void AddCertificate(CertInfo *cert) { cert->Into(&certificates); }
	const CertInfo *GetFirstCertificate() const {
		return (CertInfo *)certificates.First();
	}
	int NumCerts() const { return certificates.Cardinal(); }
	Type GetProblemType() const { return problemtype; }
	unsigned int GetReporterId() const { return reporter_id; }
	ServerName *GetServerName() { return servername; }

private:
	ServerName_Pointer servername;
	Head certificates;
	Type problemtype;
	unsigned int reporter_id;
};

#endif // CERTPROBLEM_H
