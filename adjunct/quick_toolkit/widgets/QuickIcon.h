/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_ICON_H
#define QUICK_ICON_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"

class QuickIcon : public QuickOpWidgetWrapper<OpIcon>
{
	typedef QuickOpWidgetWrapper<OpIcon> Base;
	IMPLEMENT_TYPEDOBJECT(Base);

public:
	virtual OP_STATUS Init() { RETURN_IF_ERROR(Base::Init()); SetSkin("Dialog Icon Skin"); return OpStatus::OK; }

	void	SetImage(const char* image_name) { GetOpWidget()->SetImage(image_name, TRUE); }
	void	SetBitmapImage(Image& image) { GetOpWidget()->SetBitmapImage(image, TRUE); }
	void	SetAllowScaling() { GetOpWidget()->SetAllowScaling(); }

protected:
	// Overriding QuickWidget
	virtual unsigned GetDefaultMinimumWidth() { return GetWidth(); }
	virtual unsigned GetDefaultMinimumHeight(unsigned width) { return GetHeight(); }
	virtual unsigned GetDefaultNominalWidth() { return GetWidth(); }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return GetHeight(); }
	virtual unsigned GetDefaultPreferredWidth() { return GetWidth(); }
	virtual unsigned GetDefaultPreferredHeight(unsigned width) { return GetHeight(); }

private:
	unsigned GetHeight();
	unsigned GetWidth();
};

#endif // QUICK_ICON_H
