/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_JIL_PLAYER_SUPPORT

#include "modules/dom/src/domjil/domjilvideoplayer.h"
#include "modules/dom/src/domenvironmentimpl.h"

DOM_JILVideoPlayer::DOM_JILVideoPlayer()
{

}

DOM_JILVideoPlayer::~DOM_JILVideoPlayer()
{
}

/* static */
OP_STATUS DOM_JILVideoPlayer::Make(DOM_JILVideoPlayer*& jil_video_player, DOM_Runtime* runtime)
{
	jil_video_player = OP_NEW(DOM_JILVideoPlayer, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(jil_video_player, runtime, runtime->GetPrototype(DOM_Runtime::JIL_VIDEOPLAYER_PROTOTYPE), "VideoPlayer"));
	return OpStatus::OK;
}

/* static */ int
DOM_JILVideoPlayer::open(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);

	return jilvideoplayer->Open(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILVideoPlayer::play(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);
	DOM_CHECK_ARGUMENTS_JIL("n");

	return jilvideoplayer->Play(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILVideoPlayer::stop(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);

	return jilvideoplayer->Stop(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILVideoPlayer::pause(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);

	return jilvideoplayer->Pause(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILVideoPlayer::resume(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);

	return jilvideoplayer->Resume(argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_JILVideoPlayer::setWindow(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);
	DOM_CHECK_ARGUMENTS_JIL("O");

	return jilvideoplayer->SetWindow(argv, argc, return_value, origining_runtime);
}

#ifdef SELFTEST
/* static */ int
DOM_JILVideoPlayer::getState(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jilvideoplayer, DOM_TYPE_JIL_VIDEOPLAYER, DOM_JILVideoPlayer);

	return jilvideoplayer->GetState(argv, argc, return_value, origining_runtime);
}
#endif // SELFTEST

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILVideoPlayer)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILVideoPlayer, DOM_JILVideoPlayer::open,	"open",		"s-",	"VideoPlayer.open", 0)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILVideoPlayer, DOM_JILVideoPlayer::play,		"play",		"n-",	"VideoPlayer.play")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILVideoPlayer, DOM_JILVideoPlayer::stop,		"stop",		"",		"VideoPlayer.stop")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILVideoPlayer, DOM_JILVideoPlayer::pause,		"pause",	"",		"VideoPlayer.pause")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILVideoPlayer, DOM_JILVideoPlayer::resume,	"resume",	"",		"VideoPlayer.resume")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILVideoPlayer, DOM_JILVideoPlayer::setWindow,	"setWindow","-",	"VideoPlayer.setWindow")
#ifdef SELFTEST
	DOM_FUNCTIONS_FUNCTION(DOM_JILVideoPlayer, DOM_JILVideoPlayer::getState,		"getState",	"")
#endif // SELFTEST
DOM_FUNCTIONS_END(DOM_JILVideoPlayer)

#endif // MEDIA_JIL_PLAYER_SUPPORT
