/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_BUTTON_H
#define QUICK_BUTTON_H

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "modules/widgets/OpButton.h"

class OpInputAction;
class OpWidgetImage;

class QuickButton : public QuickTextWidgetWrapper<OpButton>
{
	typedef QuickTextWidgetWrapper<OpButton> Base;
	IMPLEMENT_TYPEDOBJECT(Base);

public:
	QuickButton(): m_content(0) {}
	~QuickButton() { OP_DELETE(m_content); }

	/** Set skin image to use on this button
	  * @param image_name Name of image in skin
	  */
	OP_STATUS SetImage(const char* image_name);

	/**
	 * Set bitmap image to use on this button.
	 *
	 * @param image the image
	 */
	OP_STATUS SetImage(Image image);

	/** Set alternative contents for this button
	  * @param content Content to appear on button. Takes ownership
	  */
	OP_STATUS SetContent(QuickWidget* content);
	
	/** Sets action to use when this button is clicked
	  * 
	  * @action the action to use.  A copy is made.
	  *
	  * @deprecated Action copying doesn't handle combined actions.  Use
	  * GetOpWidget()->SetAction() instead.
	  */
	DEPRECATED(OP_STATUS SetAction(const OpInputAction* action));

	/** @return Action set on button
	  * 
	  * @deprecated Use GetOpWidget()->GetAction() instead.
	  */
	DEPRECATED(OpInputAction* GetAction());

	/** Set whether this button should be the default button
	  * @param is_default Whether the button is the default button
	  */
	void SetDefault(BOOL is_default) { GetOpWidget()->SetDefaultLook(is_default); }

	// Override QuickWidget
	virtual OP_STATUS Layout(const OpRect& rect);

	static QuickButton* ConstructButton(Str::LocaleString string_id, OpInputAction* action);
	static QuickButton* ConstructButton(OpInputAction* action);

protected:
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);

private:
	static void MakeDescendantsNotRespondToInput(OpWidget* widget);
	OpWidgetImage* PrepareSkin();

	QuickWidget* m_content;
};

#endif // QUICK_BUTTON_H
