/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SKIN_CAPABILITIES_H
#define SKIN_CAPABILITIES_H

// Use syntax: SKIN_CAP_xxxxxx
#define SKIN_CAP_SIZE_ENUM
#define SKIN_CAP_HILDON_EXTENSIONS
#define SKIN_CAP_OPWIDGETIMAGE 			// Has class OpWidgetImage
#define SKIN_CAP_SETEFFECTBITMAPIMAGE	// Has method SetEffectBitmapImage & friends
#define SKIN_CAP_ANIMATION_LISTENER		// Has Set/RemoveAnimationListener methods
#define SKIN_CAP_ELEMENT_GETIMAGE		// OpSkinElement has GetImage
#define SKIN_CAP_DRAW_SKIN_PART			// Has DrawSkinPart
#define SKIN_CAP_NATIVE_MOVED			// WindowsOpSkinElement.(cpp|h) moved to platform code and NativeOpSkinElement.* moved to adjunct
#define SKIN_CAP_BACKGROUND_COLOR		// GetBackgroundColor() within OpSkinManager and OpSkinElement
#define SKIN_CAP_TEXTSHADOW				// Has OpSkinTextShadow
#define SKIN_CAP_MORE_SKINELEMENT_METHODS	// OpWidgetImage::HasSkinElement() is public, OpWidgetImage::HasFallbackElement() and OpWidgetImage::GetFallbackElement() are available

#endif // !SKIN_CAPABILITIES_H
