/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLTOKENBACKEND_H
#define XMLTOKENBACKEND_H

#include "modules/xmlutils/xmltoken.h"

/**

  An XMLTokenBackend is used to provide literal data, and optionally
  location information, on demand.  To avoid unnecessary copying and
  other work, some information available via the XMLToken interface is
  owned by the XML parser, or whatever the source of the token is, and
  not realized until needed.  The token source implements the
  XMLTokenBackend to accomplish this.

  All functions in the interface are called in the context of a an
  XMLToken, which is the token currently being processed in a call to
  XMLTokenHandler::HandleToken.  That is, the XMLTokenBackend object
  is connected to the object that calls XMLTokenHandler::HandleToken
  (the might for instance be the same object.)  For this reason, the
  information in an XMLToken supplied by the XMLTokenBackend is
  typically not available except during the call to
  XMLTokenHandler::HandleToken.

  This interface is not something that typically needs to be
  implemented or used by code simply handling XML in regular ways.

*/
class XMLTokenBackend
{
public:
	virtual ~XMLTokenBackend();
	/**< Destructor. */

	virtual BOOL GetLiteralIsWhitespace() = 0;
	/**< Should return TRUE if the current literal consists only of
	     whitespace characters (U+0009, U+000A, U+000D or U+0020). */

	virtual const uni_char *GetLiteralSimpleValue() = 0;
	/**< Should return the current literal value as a single
	     contiguous string.  The string need not be null terminated,
	     and should be owned by the backend.  It must be valid at
	     least until the current call to a XMLTokenHandler returns.
	     If no such string is available, NULL should be returned. */

	virtual uni_char *GetLiteralAllocatedValue() = 0;
	/**< Should return the current literal value as a single
	     contiguous null terminated string.  The string will be owned
	     by the caller and must be allocated with array new.  NULL
	     should be returned if the allocation fails and for no other
	     reason. */

	virtual unsigned GetLiteralLength() = 0;
	/**< Should return the length of the current literal value, in
	     16-bit characters.  Surrogate pairs count as two characters,
	     as usual. */

	virtual OP_STATUS GetLiteral(XMLToken::Literal &literal) = 0;
	/**< Should set the current literal value in the supplied
	     XMLToken::Literal object using the functions
	     XMLToken::Literal::SetPartsCount and
	     XMLToken::Literal::SetPart.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual void ReleaseLiteralPart(unsigned index) = 0;
	/**< When called for a part previously set as owned in an
	     XMLToken::Literal object by the most recent call to
	     XMLTokenBackend::GetLiteral, the backend should release
	     ownership of the part and consequenty not free it later.
	     FIXME: Could that be any more unclear? */

#ifdef XML_ERRORS
	virtual BOOL GetTokenRange(XMLRange &range) = 0;
	/**< Should set the supplied XMLRange object to represent the
	     whole token and return TRUE.  If this is not possible, the
	     supplied XMLRange object should not be modified, and FALSE
	     should be returned.  */

	virtual BOOL GetAttributeRange(XMLRange &range, unsigned index) = 0;
	/**< Should set the supplied XMLRange object to represent the
	     index-th attribute (name, equal sign and value, including the
	     quotes around the value) and return TRUE.  If this is not
	     possible, the supplied XMLRange object should not be
	     modified, and FALSE should be returned. */
#endif // XML_ERRORS
};

#endif // XMLTOKENBACKEND_H
