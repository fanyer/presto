/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
** File : OpPersona.h
**
**
*/

#ifndef OPPERSONA_H
#define OPPERSONA_H

#ifdef PERSONA_SKIN_SUPPORT

#include "modules/skin/skin_listeners.h"
#include "modules/skin/OpSkin.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/opfile/opfile.h"
#include "modules/img/image.h"

/**
* This class is the wrapper for a "persona", a simplified version of a skin
* that modifies the loaded skin in certain ways.  It applied on top of
* whatever skin is the active skin.
* It will colorize the loaded skin with an average color or a color
* defined in the persona's manifest persona.ini. 
* The background image defined in the persona is used to paint the background
* of the window.
*
*/
class OpPersona : public OpSkin
{
public:
	OpPersona();
	virtual	~OpPersona();

	/** Init the skin from the given file. Return OpStatus::OK if successfully loaded. */
	virtual OP_STATUS InitL(OpFileDescriptor* skinfile);

	/** Wrapper methods to get the "Window Image" skin element and skin element image */
	Image GetBackgroundImage();
	OpSkinElement* GetBackgroundImageSkinElement();

	// SkinNotificationListener
	void OnSkinLoaded(OpSkin *loaded_skin);

	/** should only be handled for normal skins: */
	void OnPersonaLoaded(OpPersona *loaded_persona) {}

	/** Is our element in the (short) clear elements list */
	bool IsInClearElementsList(const OpStringC8& name);

protected:
	/** we don't want to use any default color scheme for the persona image */
	virtual void UpdateCurrentDefaultColorScheme() { }

private:

	void FreeClearElementsList();

	// parse all elements of the persona.ini's [Clear elements] section
	OP_STATUS GenerateClearElementsList();

	OpVector<char>	m_clear_elements;		// Will contain just a handful of elements that the persona will modify in any loaded skin
};
#endif // PERSONA_SKIN_SUPPORT

#endif // !OPPERSONA_H
