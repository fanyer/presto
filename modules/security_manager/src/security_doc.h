/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file security_doc.cpp
 * Cross-module security manager: DOC model.
 *
 * See document/xxx.txt
 */

#ifndef SECURITY_DOC_H
#define SECURITY_DOC_H

class OpSecurityContext;
class OpSecurityState;

#include "modules/logdoc/logdocenum.h" // InlineResourceType

class OpSecurityContext_DOC
{
protected:
	const char *m_charset;
	BOOL inline_element;
	FramesDocument *doc;
	InlineResourceType inline_type;
	BOOL from_user_css;

	friend class OpSecurityManager_DOC;

public:
	OpSecurityContext_DOC() : m_charset(NULL), inline_element(FALSE), doc(NULL), inline_type(INVALID_INLINE), from_user_css(FALSE){}
	FramesDocument* GetDoc() const { return doc; }
	InlineResourceType GetInlineType() const { return inline_type; }
	BOOL IsFromUserCss() const { return from_user_css; }
};

class OpSecurityManager_DOC
{
protected:
	/**
	 * Check that the a site (not included in the check) can inherit the preferred charset attribute
	 * from the target context.
	 *
	 * @param[in] source The security context containing the URL and the charset of the parent document
	 * (the document the charset is to be inherited from).
	 * @param[in] target The security context containing the URL of a document or an element which is meant
	 * to inherit the charset and the charset of the parent document.
	 * @param[out] allowed TRUE if the charset inheritance is allowed. Otherwise FALSE.
	 *
	 * @return OpStatus::OK
	 */
	OP_STATUS CheckPreferredCharsetSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);
	OP_STATUS CheckInlineSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state);
};

#endif // SECURITY_DOC_H
