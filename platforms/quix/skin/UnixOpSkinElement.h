/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UNIX_OPSKINELEMENT_H
#define UNIX_OPSKINELEMENT_H

#include "modules/skin/OpSkinElement.h"
#include "platforms/quix/toolkits/NativeSkinElement.h"

class NativeSkinElement;
class OpBitmap;

class UnixOpSkinElement : public OpSkinElement
{
public:
	UnixOpSkinElement(NativeSkinElement* native_element, OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);

	~UnixOpSkinElement();

	// From OpSkinElement
	virtual OP_STATUS Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args);
	virtual void OverrideDefaults(OpSkinElement::StateElement* se);
	virtual OP_STATUS GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);

	OpBitmap* GetBitmap(UINT32 width, UINT32 height, INT32 state);

private:
	NativeSkinElement* m_native_element;
	OpBitmap* m_bitmaps[NativeSkinElement::STATE_COUNT];
};

#endif // UNIX_OPSKINELEMENT_H
