/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CURSOR_MAC_H
#define CURSOR_MAC_H

#include "modules/display/cursor.h"

class MouseCursor			// the 'Cursor' structure has been taken already on the Macintosh!
{
public:
				MouseCursor();			// load/initialize cursor array
				~MouseCursor();			// release all nscursors form cursors_table
	void		SetCursor(CursorType newCursor);
	void        ReapplyCursor();
	CursorType	GetCursor(){ return m_current_cursor; }
private:
	CursorType			m_current_cursor;
	void*				m_cursors_table[CURSOR_NUM_CURSORS];
};

extern	MouseCursor	*gMouseCursor;

#endif // !CURSOR_MAC_H
