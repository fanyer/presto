/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILVIDEOPLAYER_H
#define DOM_DOMJILVIDEOPLAYER_H

#if defined MEDIA_JIL_PLAYER_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domjil/domjilmediaplayer.h"

class DOM_Element;

class DOM_JILVideoPlayer : public DOM_JILMediaPlayer
{
public:
	~DOM_JILVideoPlayer();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_VIDEOPLAYER || DOM_Object::IsA(type); }

	static OP_STATUS Make(DOM_JILVideoPlayer*& new_jil_videoplayer, DOM_Runtime* runtime);

	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(play);
	DOM_DECLARE_FUNCTION(stop);
	DOM_DECLARE_FUNCTION(pause);
	DOM_DECLARE_FUNCTION(resume);
	DOM_DECLARE_FUNCTION(setWindow);
#ifdef SELFTEST
	DOM_DECLARE_FUNCTION(getState);
#endif // SELFTEST

		enum { FUNCTIONS_ARRAY_SIZE = 7
#ifdef SELFTEST
	+ 1
#endif // SELFTEST
	};

private:
	DOM_JILVideoPlayer();
};

#endif // MEDIA_JIL_PLAYER_SUPPORT

#endif // DOM_DOMJILVIDEOPLAYER_H
