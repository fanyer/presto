/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_GLOB_NODE_H__
#define __FILEHANDLER_GLOB_NODE_H__

enum GlobType
{
    GLOB_TYPE_SIMPLE        = 0,
    GLOB_TYPE_LITERAL       = 1,
    GLOB_TYPE_COMPLEX       = 2,
    NUM_GLOB_TYPES          = 3 //Do not insert constants after this one
};

class GlobNode
{
public:

	const uni_char * GetKey() { return m_glob.HasContent() ? uni_stripdup(m_glob.CStr()) : 0; }

    GlobNode(const OpStringC & glob,
			 const OpStringC & mime_type,
			 GlobType type);

    ~GlobNode();

    const OpStringC & GetMimeType() { return m_mime_type; }
    const OpStringC & GetGlob()     { return m_glob; }

private:
    OpString m_glob;
    OpString m_mime_type;
    GlobType m_type;
};

#endif //__FILEHANDLER_GLOB_NODE_H__
