/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_MIME_TYPE_NODE_H__
#define __FILEHANDLER_MIME_TYPE_NODE_H__

#include "platforms/viewix/src/nodes/FileHandlerNode.h"
#include "platforms/viewix/src/StringHash.h"

class ApplicationNode;

class MimeTypeNode : public FileHandlerNode
{
public:

    MimeTypeNode(const OpStringC & mime_type);
    ~MimeTypeNode();

    NodeType GetType() { return MIME_TYPE_NODE_TYPE; }

	OpVector<OpString> & GetMimeTypes() { return m_mime_types; }

	OpString * AddMimeType(const OpStringC & mime_type);

	OpString * AddSubclassType(const OpStringC & mime_type);

    OP_STATUS AddApplication(ApplicationNode * app);

    void SetDefault(ApplicationNode * app);

    ApplicationNode * GetDefaultApp() { return m_default_app; }

	OP_STATUS GetDefaultIconName(OpString & icon);

    OpHashTable* m_applications;

private:

	void RemoveApplication(ApplicationNode * app);

    StringHash          m_hash_function;
    ApplicationNode*    m_default_app;
	OpAutoVector<OpString> m_mime_types;
	OpAutoVector<OpString> m_subclass_types;
};

#endif //__FILEHANDLER_MIME_TYPE_NODE_H__
