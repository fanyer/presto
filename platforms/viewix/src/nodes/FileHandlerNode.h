/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_NODE_H__
#define __FILEHANDLER_NODE_H__

enum NodeType
{
    NODE_TYPE,
    APPLICATION_NODE_TYPE,
    MIME_TYPE_NODE_TYPE,
	THEME_NODE_TYPE
};

class FileHandlerNode
{
protected:
    FileHandlerNode(const OpStringC & desktop_file_name,
					const OpStringC & icon);

    virtual ~FileHandlerNode();

public:

    void SetIcon(const OpStringC & icon){ m_icon.Set(icon);}

    const OpStringC & GetIcon() { return m_icon;}

    void SetIconPath(const OpStringC & icon_path, UINT32 icon_size = 16);

    const uni_char * GetIconPath(UINT32 icon_size = 16){ return (icon_size == m_icon_size) ? m_icon_path.CStr() : 0; }

    OpBitmap * GetBitmapIcon(UINT32 icon_size = 16);

    void SetDesktopFileName(const OpStringC & desktop_file_name) { m_desktop_file_name.Set(desktop_file_name); }

    const OpStringC & GetDesktopFileName()   { return m_desktop_file_name; }

    void SetComment(const OpStringC & comment){ m_comment.Set(comment); }

    const OpStringC & GetComment() { return m_comment; }

    virtual void SetName(const OpStringC & name){ m_name.Set(name); }

    const OpStringC & GetName() { return m_name;}

    BOOL3 HasDesktopFile() { return m_has_desktop_file; }

    void SetHasDesktopFile(BOOL3 has_desktop_file) { m_has_desktop_file = has_desktop_file; }

    BOOL3 HasIcon(UINT32 icon_size = 16) { return ((icon_size == m_icon_size) || (m_icon_size == 0)) ? m_has_icon : (BOOL3) MAYBE; }

    void SetHasIcon(BOOL3 has_icon) { m_has_icon = has_icon; }

    virtual NodeType GetType() { return NODE_TYPE; }

private:

    OpString m_icon;
    OpString m_icon_path;
    UINT32   m_icon_size;
    OpString m_desktop_file_name;
    OpString m_comment;
    OpString m_name;
    BOOL3    m_has_desktop_file;
	BOOL3    m_has_icon;
};

#endif //__FILEHANDLER_NODE_H__
