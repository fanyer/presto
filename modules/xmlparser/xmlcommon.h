/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLCOMMON_H
#define XMLPARSER_XMLCOMMON_H

#include "modules/xmlparser/xmlconfig.h"
#include "modules/xmlutils/xmltypes.h"

#ifdef XML_DEBUGGING
# define XML_OBJECT_CREATED(type) (++g_XMLObject_count, ++g_##type##_count)
# define XML_OBJECT_DESTROYED(type) (--g_XMLObject_count, --g_##type##_count)

extern unsigned g_XMLObject_count;
extern unsigned g_XMLInternalParser_count;
extern unsigned g_XMLInternalParserState_count;
extern unsigned g_XMLDoctype_count;
extern unsigned g_XMLDoctypeAttribute_count;
extern unsigned g_XMLDoctypeElement_count;
extern unsigned g_XMLDoctypeEntity_count;
extern unsigned g_XMLDoctypeNotation_count;
extern unsigned g_XMLPEInGroup_count;
extern unsigned g_XMLContentSpecGroup_count;
extern unsigned g_XMLBuffer_count;
extern unsigned g_XMLBufferState_count;
extern unsigned g_XMLDataSource_count;
#else // XML_DEBUGGING
# define XML_OBJECT_CREATED(type)
# define XML_OBJECT_DESTROYED(type)
#endif // XML_DEBUGGING

#endif // XMLPARSER_XMLCOMMON_H
