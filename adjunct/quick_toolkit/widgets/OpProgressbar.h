/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PROGRESSBAR_H
#define OP_PROGRESSBAR_H

#include "adjunct/quick_toolkit/widgets/OpLabel.h"

class OpButton;

/*****************************************************************************
**
**	OpProgressBar
**
*****************************************************************************/

class OpProgressBar : public OpLabel
{
	public:
	enum ProgressType
	{
		Normal,
		Only_Label,
		Spinner,
		Percentage_Label //< Normal progress bar, with percentage set as its label automatically
	};

	protected:
		OpProgressBar();
		~OpProgressBar();

	public:
		static OP_STATUS Construct(OpProgressBar** obj);

		OpFileLength GetCurrentStep() const { return m_current_step; }
		OpFileLength GetTotalSteps() const { return m_total_steps; }

		void SetPercentageLabel();

		virtual OP_STATUS SetText(const uni_char* text);
		virtual OP_STATUS SetLabel(const uni_char* newlabel, BOOL center = FALSE); 

		// if total steps is 0 AND steps is non zero, then draw a just a generic progress

		void SetProgress(OpFileLength current_step, OpFileLength total_steps);
		
		void SetType(ProgressType type);// { m_type = type; }

		// Set that skin image that use for the spinner type
		OP_STATUS SetSpinnerImage(const char* skin_image) { return m_spinner_image.Set(skin_image); }

		 ///  Deprecated SetShowOnlyLabel... Stop using this use SetType instead
		void SetShowOnlyLabel(BOOL only_label) { only_label ? m_type = Only_Label : m_type = Normal; } 

		void RunIndeterminateBar(UINT32 ms = 50);
		void StopIndeterminateBar();
		OP_STATUS	SetProgressBarEmptySkin(const char *skin);
		OP_STATUS	SetProgressBarFullSkin(const char *skin);


		// == Hooks ======================
		virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
		virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
		virtual void OnFontChanged() { m_widget_string.NeedUpdate(); }
		virtual void OnDirectionChanged() { OnFontChanged(); }

		// Implementing the OpTreeModelItem interface
		virtual Type GetType() { return WIDGET_TYPE_PROGRESSBAR; }
		
		// OpTimerListener
		virtual void OnTimeOut(OpTimer* timer);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindView;}
#endif

		virtual void OnScaleChanged();

	private:
		OpFileLength	m_current_step;		// Steps we have completed so far.
		OpFileLength	m_total_steps;		// Total number of steps.
		OpTimer			*m_timer;
		UINT32			m_ms;
		ProgressType	m_type;				// Type of progress bar
		OpString8		m_spinner_image;	// Custom image used for spinner type
		OpString8		m_progressbar_skin_empty;
		OpString8		m_progressbar_skin_full;
		
		// Local holder so that the string can be draw in different colours
		OpWidgetString			m_widget_string;
};

#endif // OP_PROGRESSBAR_H
