/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILAUDIOPLAYER_H
#define DOM_DOMJILAUDIOPLAYER_H

#ifdef MEDIA_JIL_PLAYER_SUPPORT

#include "modules/dom/src/domjil/domjilmediaplayer.h"
#include "modules/dom/src/domcore/element.h"

class DOM_JILAudioPlayer : public DOM_JILMediaPlayer
{
public:

	virtual ~DOM_JILAudioPlayer();

	static OP_STATUS Make(DOM_JILAudioPlayer*& jil_audio_player, DOM_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_AUDIOPLAYER || DOM_Object::IsA(type); }

	/* Override the Link functions so that they always operate on the Link
	 * object instead of ES_ThreadListener.
	 */
	void Into(Head* head)   { Link::Into(head); }
	void Out()              { Link::Out(); }

	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(play);
	DOM_DECLARE_FUNCTION(stop);
	DOM_DECLARE_FUNCTION(pause);
	DOM_DECLARE_FUNCTION(resume);
#ifdef SELFTEST
	DOM_DECLARE_FUNCTION(getState);
#endif // SELFTEST

	enum { FUNCTIONS_ARRAY_SIZE = 6
#ifdef SELFTEST
	+ 1
#endif // SELFTEST
	};

private:
	DOM_JILAudioPlayer();
};
#endif // MEDIA_JIL_PLAYER_SUPPORT

#endif // DOM_DOMJILAUDIOPLAYER_H
