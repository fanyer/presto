/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_THEME_NODE_H__
#define __FILEHANDLER_THEME_NODE_H__

#include "platforms/viewix/src/nodes/FileHandlerNode.h"

class ThemeNode : public FileHandlerNode
{
public:
    ThemeNode() : FileHandlerNode(0, 0) {}
    ~ThemeNode() {}

    NodeType GetType() { return THEME_NODE_TYPE; }

    OP_STATUS SetInherits(const OpStringC & inherits) { return m_inherits.Set(inherits); }

    const OpString & GetInherits() const { return m_inherits; }

    OP_STATUS SetIndexThemeFile(const OpStringC & index_theme_file) { return m_index_theme_file.Set(index_theme_file); }

    const OpString & GetIndexThemeFile() const { return m_index_theme_file; }

    OP_STATUS SetThemePath(const OpStringC & theme_path) { return m_theme_path.Set(theme_path); }

    const OpString & GetThemePath() const { return m_theme_path; }

private:

    OpString m_inherits;
    OpString m_index_theme_file;
    OpString m_theme_path;
};

#endif //__FILEHANDLER_THEME_NODE_H__
