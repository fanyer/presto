/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arne Martin Guettler
 */
#if defined TRUST_RATING && !defined MODULES_ABOUT_OPERAFRAUDWARNING_H
#define MODULES_ABOUT_OPERAFRAUDWARNING_H

#include "modules/url/url2.h"
#include "modules/about/opgenerateddocument.h"
#include "modules/dochand/fraud_check.h"

/**
 * Generator for the fraud warning page to which the user is redirected
 * if visiting a site which has been determined to be a phishing site.
 */
class OperaFraudWarning : public OpGeneratedDocument
{
public:
	/**
	 * @param url          URL into which the new page will be generated.
	 * @param blocked_url  URL that was blocked, used just for display.
	 * @param advisory     Advisory object with information about why the
	 *                     url was blocked.
	 */
	OperaFraudWarning(const URL& url, const URL& blocked_url, TrustInfoParser::Advisory *advisory = NULL);

	/**
	 * Generate the warning page to the specified internal URL. This will
	 * overwrite anything that has already been written to the URL, so the
	 * caller must make sure we do not overwrite anything important.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

	enum FraudType
	{
		FraudTypePhishing,
		FraudTypeMalware,
		FraudTypeUnknown
	};
protected:
	URL m_suppressed_url;  ///< The URL to the phishing site we're suppressing
	unsigned m_id;
	FraudType m_type;
};

#endif // TRUST_RATING && MODULES_ABOUT_OPERAFRAUDWARNING_H
