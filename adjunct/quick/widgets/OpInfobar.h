/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OP_INFOBAR_H
#define OP_INFOBAR_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

#include "modules/widgets/OpButton.h"

#include "modules/util/adt/opvector.h"

class PrefsFile;
class PrefsSection;
class OpButton;
class OpWindowCommander;
class OpToolbar;

/***********************************************************************************
**
**	OpInfobar
**
***********************************************************************************/

class OpInfobar : public OpToolbar
{
protected:

	OpInfobar(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref, PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);
	virtual ~OpInfobar();

public:

	static OP_STATUS	Construct(OpInfobar** obj);

	OP_STATUS			Init(const char *toolbar_name);
	void				SetInfoImage(const char *image);
	OP_STATUS			SetInfoButtonActionString(OpString8& straction);


	/**
	 * Return the name of the status field widget. Must be implemented by a subclass to have
	 * a unique name. Example: MyBar should use tbb_MyStatusText. The name must match with what
	 * is used in the resource files. The name is used by @ref SetStatusText and SetStatusTextWrap
	 * to look up the widget. Note: Legacy support allows this name not to be set but all new toolbars
	 * should set it.
	 */
	virtual const char* GetStatusFieldName() { return NULL; }

	/**
	 * Return the name of the question field widget. Must be implemented by a subclass to have
	 * a unique name. Example: MyBar should use tbb_MyQuestionText. The name must match with what
	 * is used in the resource files. A valid name is mandatory for @ref SetQuestionText and
	 * ShowQuestionText to work
	 */
	virtual const char* GetQuestionFieldName() { return NULL; }

	/**
	 * Set the status text. An OpStatusField widget named 'tbb_StatusText' is preferred, otherwise the
	 * first widget of type OpStatusField is used
	 *
	 * @param text. The text to set
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficent memory or OpStatus::ERR_NO_SUCH_RESOURCE
	 *         if no widget could be located.
	 */
	OP_STATUS			SetStatusText(OpStringC text);

	/**
	 * Set a question or additional status text. An OpStatusField widget named 'tbb_QuestionText' is required
	 *
	 * @param text. The text to set
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on insufficent memory or OpStatus::ERR_NO_SUCH_RESOURCE
	 *         if no widget could be located.
	 */
	OP_STATUS			SetQuestionText(OpStringC text);

	/**
	 * Set wrapping (multi line messages) on or off for the status text fiel. An OpStatusField widget named
	 * 'tbb_QuestionText' is preferred, otherwise the first widget of type OpStatusField is used
	 *
	 * @param wrap TRUE (multi line) or FALSE (single line)
	 */
	void				SetStatusTextWrap(BOOL wrap);

	/**
	 * Show or hide the question or additional status text area.
	 *
	 * @param show TRUE (make field visible) or FALSE (hide field)
	 */
	void				ShowQuestionText(BOOL show);

	virtual void		OnReadContent(PrefsSection *section);
	virtual Type		GetType() {return WIDGET_TYPE_INFOBAR;}

	virtual BOOL Show()			  { return SetAlignmentAnimated(OpBar::ALIGNMENT_OLD_VISIBLE); }
	virtual BOOL Hide()			  { return SetAlignment(OpBar::ALIGNMENT_OFF); }
	BOOL AnimatedHide()	  { return SetAlignmentAnimated(OpBar::ALIGNMENT_OFF); }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindToolbar;}
#endif

private:
	OP_STATUS CreateInfoButton();

protected:
	OpString			m_status_text;
	OpString			m_question_text;
	OpButton *			m_info_button;
	BOOL				m_wrap;
};

#endif // OP_TOOLBAR_H
