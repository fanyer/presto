// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

// ---------------------------------------------------------------------------------

#ifndef STRUTIL_H
#define STRUTIL_H

#include "modules/util/opstring.h"

//****************************************************************************
//
//	StringTokenizer
//
//****************************************************************************

//! A simple string tokenizers, working with OpString.

class StringTokenizer
{
public:
	// Construction.
	StringTokenizer(const OpStringC& input, const OpStringC& tokens = UNI_L(" "));

	/*! Init function. May be used several times. */
	OP_STATUS Init(const OpStringC &input);

	/*! Returns true if we still have more tokens we could fetch. */
	BOOL HasMoreTokens() const;

	/*! Get the next token available. */
	OP_STATUS NextToken(OpString &token);

	/*! Skip the current token. */
	OP_STATUS SkipToken();

	/*! Get the remainder of the string (what we have not tokenized yet).
		Useful if you just want to tokenize part of the string. */
	OP_STATUS RemainingString(OpString &string) const;

private:
	// Members.
	OpString m_input_string;
	OpStringC m_tokens;
};

#endif // STRUTIL_H
