/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPBUBBLEHANDLER_H
#define OPBUBBLEHANDLER_H

class OpBubble;
class OpWidget;
class OpWindow;

/** Handles ownership of a OpBubble. */
class OpBubbleHandler
{
public:
	OpBubbleHandler();
	~OpBubbleHandler();

	/** Take ownership of a OpBubble. Useful if desktop code wants to create and show special versions of OpBubble */
	void SetBubble(OpBubble *bubble);

	/** Create a OpBubble with the given text. */
	OP_STATUS CreateBubble(OpWindow *parent_window, const uni_char *text);

	/** Update the placement of the current bubble.
		See OpBubble::UpdatePlacement for details. */
	void UpdatePlacement(const OpRect &target_screen_rect);

	/** Update the font and colors of the current bubble.
		See OpBubble::UpdateFontAndColors for details. */
	void UpdateFontAndColors(OpWidget *source);

	/** Show bubble */
	void Show();

	/** Hide bubble */
	void Close();
private:
	friend class OpBubble;
	OpBubble *m_bubble;
	OpBubble *m_closed_bubble;
};

#endif // OPBUBBLEHANDLER_H
