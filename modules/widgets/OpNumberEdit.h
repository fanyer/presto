/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_NUMBER_EDIT_H
#define OP_NUMBER_EDIT_H

#include "modules/widgets/OpWidget.h"

class OpEdit;
class OpSpinner;

// == OpNumberEdit ==================================================

class OpNumberEdit : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:
	OpNumberEdit();

public:
	static OP_STATUS Construct(OpNumberEdit** obj);

	// Override base class
    OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);

	void SetReadOnly(BOOL readonly);

	void SelectText();

	virtual void GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction, BOOL line_normalized = FALSE);
	virtual void SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction = SELECTION_DIRECTION_DEFAULT, BOOL line_normalized = FALSE);

	INT32 GetCaretOffset();
	void SetCaretOffset(INT32 caret_ofs);

/*	void GetText(uni_char *buf);
	void GetText(uni_char *buf, INT32 buflen, INT32 offset);
	*/

	/**
	 gets the ghost text from the edit field
	 @param str (out) the target string (will be cleared)
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetGhostText(OpString &str);

	/**
	 sets the ghost text the ghost_text. a ghost text is a text that is only visible when the edit field
	 contains no text and is not focused.
	 @param ghost_text the new ghost text
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY om OOM
	 */
	OP_STATUS SetGhostText(const uni_char* ghost_text);

	/**
	 * Returns true if a value was returned.
	 */
	BOOL GetIntValue(int& out_value) const
	{
		double tmp;
		BOOL res = GetValue(tmp);
		if (res)
			out_value = (int)tmp;
		return res;
	};
	/**
	 * Returns true if a value was returned.
	 */
	BOOL GetValue(double& out_value) const;


	/**
	 * Returns TRUE if there is an input value. This doesn't mean
	 * that it's convertible to a number.
	 */
	BOOL HasValue() const;

	void SetMinValue(double new_min);
	void SetMaxValue(double new_max);
	/**
	 * This value decides together with the step base which values that are legal. Only
	 * values of the type stepbase + n*step where n is an integer are allowed.
	 * If not set it defaults to 1.0. The step must be positive.
	 */
	void SetStep(double new_step) { OP_ASSERT(new_step > 0.0); m_step = new_step; }
	/**
	 * This value decides together with the step which values that are legal. Only
	 * values of the type stepbase + n*step where n is an integer are allowed.
	 * If not set it defaults to 0.0.
	 */
	void SetStepBase(double new_step_base) { m_step_base = new_step_base; }
	OP_STATUS SetAllowedChars(const char* new_allowed_chars);

	void SetWrapAround(BOOL wrap_around);

	// == Hooks ======================
	void OnResize(INT32* new_w, INT32* new_h) { Layout(*new_w, *new_h); }
	void OnDirectionChanged() { if (GetVisualDevice()) Layout(GetWidth(), GetHeight()); }
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	BOOL OnMouseWheel(INT32 delta,BOOL vertical);
	void EndChangeProperties();

	// OpWidget
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_NUMBER_EDIT;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_NUMBER_EDIT || OpWidget::IsOfType(type); }

	// OpWidgetListener
	virtual void OnChangeWhenLostFocus(OpWidget *widget);
	virtual void OnClick(OpWidget *object, UINT32 id);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse /*= FALSE */);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual INT32 GetValue() { return 0; }
#endif // QUICK

	virtual BOOL OnInputAction(OpInputAction* action);

	/**
	 * Returns how much extra space is needed around text to display a widget that is
	 * height pixels high.
	 */
	INT32 GetExtraWidth(int height);

	/** Return the editfield that displays the number */
	OpEdit *GetEdit() { return m_edit; }
private:
	OpEdit*		 m_edit;
	OpSpinner*	 m_spinner;

	double m_min_value;
	double m_max_value;
	double m_step_base;
	double m_step;

	BOOL m_wrap_around;

	void Layout(INT32 width, INT32 height);
	void ChangeNumber(int direction);
	BOOL ParseNumber(double* return_value);

	void SetNumberValue(double value_number);
	void UpdateButtonState();
};

#endif // OP_NUMBER_EDIT_H
