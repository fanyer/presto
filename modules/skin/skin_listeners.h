/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SKIN_LISTENERS_H
#define OP_SKIN_LISTENERS_H

class OpWindow;
class OpSkin;
class OpPersona;

class TransparentBackgroundListener
{
public:
	virtual ~TransparentBackgroundListener(){}

	/* Called on all listeners when an element marked as clearing the background
	* (ClearBackground=1 on the skin element) is being drawn. 
	*
	* @param painter The painter this hook was registered for and the painter
	*      that must be used when painting anything inside the hook.
	* @param clear_rect The paint rect in client coordinates that can be painted 
	*      or cleared
	*/
	virtual void OnBackgroundCleared(OpPainter *painter, const OpRect& clear_rect) = 0;
};

class SkinNotificationListener
{
public:
	virtual ~SkinNotificationListener() { }

	/** Called on all listeners when the active skin has changed.
	*/
	virtual void OnSkinLoaded(OpSkin *loaded_skin) = 0;
#ifdef PERSONA_SKIN_SUPPORT
	/** Called on all listeners when the active persona has changed.
	*/
	virtual void OnPersonaLoaded(OpPersona *loaded_persona) = 0;
#endif // PERSONA_SKIN_SUPPORT
};


#endif // !OP_SKIN_LISTENERS_H
