/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_LABEL_H
#define OP_LABEL_H

#include "modules/widgets/OpWidget.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleOpWidgetLabel.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

class OpEdit;
class OpMultiline;

/***********************************************************************************
**
**	OpLabel
**
**  @deprecated Use DesktopLabel or OpMultilineEdit in label mode instead
***********************************************************************************/

class OpLabel : public OpWidget, public OpDelayedTriggerListener
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public AccessibleOpWidgetLabel
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
{
	public:
		static OP_STATUS	Construct(OpLabel** obj);

	protected:
		OpLabel();

	private:
		OP_STATUS			CreateEdit(BOOL wrap);

	public:
		virtual OP_STATUS	SetText(const uni_char* text);
		virtual OP_STATUS	GetText(OpString& text);

		void 				SetScrollbarStatus(WIDGET_SCROLLBAR_STATUS status);
		OP_STATUS			SetWrap(BOOL wrap);
		BOOL				HasWrap() const { return m_wrap; }
		void				SetCenterAlign(BOOL center);
		void				SetSkin(const char* skin);

		/** This makes the text inside a label selectable, and hence copiable.
		  * @param selectable If true, the text of label is selectable. Otherwise it is unselectable, i.e the way it normally is.
		  */	
		void 				SetSelectable(BOOL selectable);
		// Deprecated REMOVE IT ASAP (Do not use this use SetText and SetJustify)
		virtual OP_STATUS	SetLabel(const uni_char* newlabel, BOOL center = FALSE);

		INT32				GetPreferedHeight(UINT32 rows);
		INT32				GetTextLength();
		INT32				GetTextWidth();
		INT32				GetTextHeight();
		virtual void		GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

		void				SetUpdateDelay(INT32 initial_delay, INT32 subsequent_delay = 0) {m_delayed_trigger.SetTriggerDelay(initial_delay, subsequent_delay);}

		virtual void		SetEllipsis(ELLIPSIS_POSITION ellipsis);

		// == Hooks ======================
		virtual void		OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void		OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void		OnJustifyChanged();
		virtual void		OnDirectionChanged() { m_edit->SetRTL(GetRTL()); }
		virtual void		OnFontChanged();
		virtual void		OnResize(INT32* new_w, INT32* new_h);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindStaticText;}
		virtual OpAccessibleItem* GetLabelExtension() { return this; }

		virtual int	GetAccessibleChildrenCount() {return 0;}
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child) {return Accessibility::NoSuchChild;}
		virtual OpAccessibleItem* GetAccessibleChild(int) {return NULL;}
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		virtual Type		GetType() {return WIDGET_TYPE_LABEL;}
		virtual BOOL		IsOfType(Type type) { return type == GetType(); }

		// == OpDelayedTriggerListener ======================
		virtual void		OnTrigger(OpDelayedTrigger*) {Relayout();}

	protected:

		OpDelayedTrigger	m_delayed_trigger;

		// Below we have separated multiline and single line OpLabels to run two different types of code.
		// This is WRONG WRONG WRONG, and we need to get rid of it ASAP, but it is unavoidable at the
		// moment, since core has no one class to handle what we need so we are forced to use both
		// OpTextCollection and OpWidgetString. This code must be consolidates AS SOON AS POSSIBLE by
		// using only one new type or having OpTextCollection extended.  adamm 02-05-2007

		// Member which sets the wrapping and says which of the below functionallity to use
		BOOL				m_wrap;					// Tells the label to auto wrap when it hits its visible extents
		OpWidget			*m_edit;				// Pointer that can either be an OpEdit or OpMultlineEdit
													// depending of whether we want single or multiline support

		INT16				m_old_font_size;		// Keeps the font size since previous update, for comparison
		BOOL				m_font_size_changed;	// Did the font size change since previous reformatting?
};

#endif // OP_LABEL_H
