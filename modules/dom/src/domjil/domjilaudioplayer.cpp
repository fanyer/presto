/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_JIL_PLAYER_SUPPORT
#include "modules/dom/src/domjil/domjilaudioplayer.h"

DOM_JILAudioPlayer::DOM_JILAudioPlayer()
{
}

DOM_JILAudioPlayer::~DOM_JILAudioPlayer()
{
}

OP_STATUS DOM_JILAudioPlayer::Make(DOM_JILAudioPlayer*& jil_audio_player, DOM_Runtime* runtime)
{
	jil_audio_player = OP_NEW(DOM_JILAudioPlayer, ());
	return DOMSetObjectRuntime(jil_audio_player, runtime, runtime->GetPrototype(DOM_Runtime::JIL_AUDIOPLAYER_PROTOTYPE), "AudioPlayer");
}

/* static */ int
DOM_JILAudioPlayer::open(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilaudioplayer, DOM_TYPE_JIL_AUDIOPLAYER, DOM_JILAudioPlayer);

	return jilaudioplayer->Open(argv, argc, return_value, origining_runtime);
}



/* static */int
DOM_JILAudioPlayer::play(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilaudioplayer, DOM_TYPE_JIL_AUDIOPLAYER, DOM_JILAudioPlayer);
	DOM_CHECK_ARGUMENTS_JIL("n");

	return jilaudioplayer->Play(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILAudioPlayer::stop(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilaudioplayer, DOM_TYPE_JIL_AUDIOPLAYER, DOM_JILAudioPlayer);

	return jilaudioplayer->Stop(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILAudioPlayer::pause(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilaudioplayer, DOM_TYPE_JIL_AUDIOPLAYER, DOM_JILAudioPlayer);

	return jilaudioplayer->Pause(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILAudioPlayer::resume(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilaudioplayer, DOM_TYPE_JIL_AUDIOPLAYER, DOM_JILAudioPlayer);

	return jilaudioplayer->Resume(argv, argc, return_value, origining_runtime);
}

#ifdef SELFTEST
/* static */ int
DOM_JILAudioPlayer::getState(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilaudioplayer, DOM_TYPE_JIL_AUDIOPLAYER, DOM_JILAudioPlayer);

	return jilaudioplayer->GetState(argv, argc, return_value, origining_runtime);
}
#endif // SELFTEST

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILAudioPlayer)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILAudioPlayer, DOM_JILAudioPlayer::open,	"open",		"s-",	"AudioPlayer.open", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAudioPlayer, DOM_JILAudioPlayer::play,		"play",		"n-",	"AudioPlayer.play")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAudioPlayer, DOM_JILAudioPlayer::stop,		"stop",		"",		"AudioPlayer.stop")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAudioPlayer, DOM_JILAudioPlayer::pause,		"pause",	"",		"AudioPlayer.pause")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAudioPlayer, DOM_JILAudioPlayer::resume,	"resume",	"",		"AudioPlayer.resume")
#ifdef SELFTEST
	DOM_FUNCTIONS_FUNCTION(DOM_JILAudioPlayer, DOM_JILAudioPlayer::getState,		"getState",	"")
#endif // SELFTEST
DOM_FUNCTIONS_END(DOM_JILAudioPlayer)

#endif // MEDIA_JIL_PLAYER_SUPPORT
