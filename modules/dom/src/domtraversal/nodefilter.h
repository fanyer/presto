/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_NODEFILTER_H
#define DOM_NODEFILTER_H

#ifdef DOM2_TRAVERSAL

class ES_Object;
class DOM_Runtime;

class DOM_NodeFilter
{
private:
	/* Shouldn't create objects of this type. */
	DOM_NodeFilter();
	DOM_NodeFilter(const DOM_NodeFilter &);

public:
	enum
	{
		/* Internal. */
		FILTER_OOM = -4,
		FILTER_EXCEPTION = -3,
		FILTER_FAILED = -2,
		FILTER_DELAY = -1,

		/* Defined by DOM. */
		FILTER_ACCEPT = 1,
		FILTER_REJECT,
		FILTER_SKIP
	};

	enum
	{
		SHOW_ALL                       = 0xfffffffful, // Might be stored as -1 depending on compiler.
		SHOW_ELEMENT                   = 0x00000001ul,
		SHOW_ATTRIBUTE                 = 0x00000002ul,
		SHOW_TEXT                      = 0x00000004ul,
		SHOW_CDATA_SECTION             = 0x00000008ul,
		SHOW_ENTITY_REFERENCE          = 0x00000010ul,
		SHOW_ENTITY                    = 0x00000020ul,
		SHOW_PROCESSING_INSTRUCTION    = 0x00000040ul,
		SHOW_COMMENT                   = 0x00000080ul,
		SHOW_DOCUMENT                  = 0x00000100ul,
		SHOW_DOCUMENT_TYPE             = 0x00000200ul,
		SHOW_DOCUMENT_FRAGMENT         = 0x00000400ul,
		SHOW_NOTATION                  = 0x00000800ul
	};

	/** Initialize the global variable "NodeFilter". */
	static void ConstructNodeFilterObjectL(ES_Object *object, DOM_Runtime *runtime);
};

#endif // DOM2_TRAVERSAL
#endif // DOM_NODEFILTER_H
