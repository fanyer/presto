/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_FILEREADERSYNC_H
#define DOM_FILEREADERSYNC_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domfile/domfilereader.h"

#ifdef DOM_WEBWORKERS_SUPPORT
class DOM_FileReaderSync_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_FileReaderSync_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::FILEREADERSYNC_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};
#endif // DOM_WEBWORKERS_SUPPORT

/**
 * The FileReaderSync interface in the W3C File API.
 */
class DOM_FileReaderSync
	: public DOM_Object,
	  public DOM_FileReader_Base
{
protected:
	DOM_FileReaderSync()
	{
	}

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FILEREADERSYNC || DOM_Object::IsA(type); }
	virtual void GCTrace();

public:
	static OP_STATUS Make(DOM_FileReaderSync*& reader, DOM_Runtime* runtime);

	DOM_DECLARE_FUNCTION_WITH_DATA(readAs);
	enum {
		FUNCTIONS_readAsArrayBuffer = 1,
		FUNCTIONS_readAsBinaryString,
		FUNCTIONS_readAsText,
		FUNCTIONS_readAsDataURL,

		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

#endif // DOM_FILEREADERSYNC_H
