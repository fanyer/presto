/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PROGRESS_H
#define OP_PROGRESS_H

#include "modules/widgets/OpWidget.h"

/** OpProgress is a widget showing progress (just a value on a linear scale).
	There's no way to change its value by interaction with it.
	It can be in either progress or meter type (which results in different skin and value sanitation)
*/

class OpProgress : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:

	OpProgress();
	virtual	~OpProgress();

public:
	static OP_STATUS Construct(OpProgress** obj);

	enum TYPE {
		TYPE_PROGRESS,			///< Progress type (the default)
		TYPE_METER_GOOD_VALUE,	///< Meter with a good value
		TYPE_METER_BAD_VALUE,	///< Meter with a bad value
		TYPE_METER_WORST_VALUE	///< Meter with a really bad value
	};
	/** Set which mode to display */
	void SetType(TYPE type);

	/** Set the current progress value (in percent) */
	void SetValue(INT32 value) { SetProgress((float)value / 100.0f); }

	/** Get the current progress value (in percent) */
	INT32 GetValue() { return (INT32)(m_progress * 100); }

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	/** Set the current progress value.
		@param val Should be a value from 0 to 1, or -1 (which means that progress bar is indeterminate) */
	void SetProgress(float progress);

	// == OpWidget ======================
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}

	// == OpWidgetListener ==============
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

private:
	TYPE m_type;
	float m_progress;
};

#endif // OP_PROGRESS_H
