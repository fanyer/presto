/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLVALIDATOR_H
#define XMLVALIDATOR_H

#ifdef XML_VALIDATING

#include "modules/xmlutils/xmltypes.h"

class XMLTokenHandler;
class XMLSerializer;

class XMLValidator
{
public:
	class Listener
	{
	public:
		enum Type
		{
			TYPE_FATAL,
			/**< Fatal error.  Validation will not be possible, or
			     will not be completely accurate.  For instance
			     signalled when doing XML document type validation of
			     a document that lacks a document type declaration. */

			TYPE_ERROR,
			/**< Validation error. */

			TYPE_WARNING,
			/**< Validation warning. */

			TYPE_COMPATIBILITY,
			/**< Signalled for issues that may affect the document's
			     compatibility with other software (according to some
			     specification.) */
		};

		enum Action
		{
			ACTION_CONTINUE,
			/**< Continue validation. */

			ACTION_ABORT
			/**< Abort validation.  No more calls will be made to
			     ValidationMessage.  ValidationStopped is called. */
		};

		virtual Action ValidationMessage(Type type, const XMLRange &position, const char *description);
		/**< Called during validation to signal issues found. */

		enum Status
		{
			STATUS_VALID,
			/**< The document was valid. */

			STATUS_INVALID,
			/**< The document was not valid. */

			STATUS_FAILED,
			/**< Validation failed. */

			STATUS_ABORTED
			/**< Validation was aborted by the listener (that is,
			     ValidationMessage return ACTION_ABORT.) */
		};

		virtual void ValidationStopped(Status status);
		/**< Called when validation is stopped (that is, when no more
		     calls to ValidationMessage is to be expected.) */
	};

	virtual OP_STATUS MakeValidatingTokenHandler(XMLTokenHandler *&tokenhandler, XMLTokenHandler *secondary, Listener *listener) = 0;
	/**< Creates a token handler that validates the document token by
	     token.  If 'secondary' is not null, tokens will be passed on
	     to it in addition to being validated.  Depending on the type
	     of validation, the tokens passed to the secondary token
	     handler may not be identical to those received by the
	     validating token handler.

	     @param tokenhandler Set to the newly created token handler.
	     @param secondary Secondary tokenhandler that will receive all
	                      tokens this token handler receives, possibly
	                      modified.
	     @return OpStatus::OK, OpStatus::ERR (if this type of
	             validation is not supported by this validator) or
	             OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS MakeValidatingSerializer(XMLSerializer *&serializer, Listener *listener) = 0;
	/**< Creates a serializer that "serializes" by validating the
	     elements serialized.  Depending on the type of validation,
	     the serialized tree may be modified during the serialization.

	     @param tokenhandler Set to the newly created serializer.
	     @return OpStatus::OK, OpStatus::ERR (if this type of
	             validation is not supported by this validator) or
	             OpStatus::ERR_NO_MEMORY. */
};

#endif // XML_VALIDATING
#endif // XMLVALIDATOR_H
