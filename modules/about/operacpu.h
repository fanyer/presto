/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/


#ifndef OPERACPU_H
#define OPERACPU_H

#ifdef CPUUSAGETRACKING

#include "modules/about/opgenerateddocument.h"

class OperaCPU : public OpGeneratedDocument
{
public:
	/**
	 * @param url URL to write to.
	 */
	OperaCPU(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	virtual OP_STATUS GenerateData();
};
#endif // CPUUSAGETRACKING

#endif // !OPERACPU_H
