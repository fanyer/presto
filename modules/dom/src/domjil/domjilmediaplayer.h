/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILMEDIAPLAYER_H
#define DOM_DOMJILMEDIAPLAYER_H

#ifdef MEDIA_JIL_PLAYER_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/media/jilmediaelement.h"
#include "modules/media/mediaplayer.h"

class DOM_JILMediaPlayer : public DOM_JILObject, public JILMediaListener, public ES_ThreadListener
{
public:
	DOM_JILMediaPlayer();

	virtual ~DOM_JILMediaPlayer();

	enum PlayerState
	{
		PLAYER_STATE_FIRST = 0,
		PLAYER_STATE_BEGIN = PLAYER_STATE_FIRST,
		PLAYER_STATE_OPENED,
		PLAYER_STATE_PLAYING,
		PLAYER_STATE_STOPPED,
		PLAYER_STATE_PAUSED,
		PLAYER_STATE_COMPLETED,
		PLAYER_STATE_LAST
	};

	BOOL IsPlaying() { return m_state == PLAYER_STATE_PLAYING; }

	/** Callback invoked when requested media resource was either
		successfully opened or error occured */
	virtual void OnOpen(BOOL error);

	/** Callback invoked after playback was sucessfully started
	(or resumed after being paused) */
	virtual void OnPlaybackStart() { SetState(PLAYER_STATE_PLAYING); }

	/** Callback invoked after playback was stopped */
	virtual void OnPlaybackStop() { SetState(PLAYER_STATE_STOPPED); };

	/** Callback invoked after playback was paused */
	virtual void OnPlaybackPause() { SetState(PLAYER_STATE_PAUSED); };

	/** Callback invoked on playback end */
	virtual void OnPlaybackEnd();

	/** From ES_ThreadListener */
	virtual OP_STATUS Signal(ES_Thread* thread, ES_ThreadSignal signal);

	/** Called when DOM Environment is going to be shut down */
	virtual void OnBeforeEnvironmentDestroy();

protected:

	/* Marks owned ES objects as used */
	virtual void GCTrace();

	/* Sets player state */
	void SetState(PlayerState state);

	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	/* Open a media resource*/
	virtual int Open(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/* Start playback*/
	virtual int Play(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/* Pause playback */
	virtual int Pause(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/* Resume playback */
	virtual int Resume(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/* Stop Playback */
	virtual int Stop(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

#ifdef SELFTEST
	/* Return media player state */
	virtual int GetState(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);
#endif // SELFTEST

	/* Sets HTML element for displaying media content */
	virtual int SetWindow(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/* Sets HTML element for displaying media content */
	OP_STATUS SetWindowInternal(DOM_Element *dom_element);

private:
	/* Player state */
	PlayerState m_state;

	/* ES callback invoked when player state has changed */
	ES_Object* m_on_mediaplayer_state_changed;

	/* Player state strings */
	const uni_char* m_state_strings[PLAYER_STATE_LAST];

	/* Thread used for opening a media */
	ES_Thread* m_open_thread;

	/* Media opening status */
	BOOL m_open_failed;

	enum { INFINITE_LOOP = -1 };

	int m_loops_left;

	/* Element responsible for painting handled media */
	DOM_Node* m_element;

	/* Returns media element stored in 'm_element' */
	JILMediaElement *GetJILMediaElement(BOOL construct = TRUE);
};

#endif // MEDIA_JIL_PLAYER_SUPPORT

#endif // DOM_DOMJILMEDIAPLAYER_H
