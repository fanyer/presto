/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPERAUNITE_H
#define OPERAUNITE_H

#ifdef OPERAUNITE_URL
#include "modules/about/opgenerateddocument.h"

class OperaUnite : public OpGeneratedDocument
{
public:
	OperaUnite(URL url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
	{}

	virtual ~OperaUnite() {};

	/**
	* Generate the opera:unite document to the specified internal URL.
	*
	* @return OK on success, or any error reported by URL or string code.
	*/
	OP_STATUS GenerateData();
};
#endif

#endif // !OPERAUNITE_H
