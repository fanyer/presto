/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_ELEMENT_STATE_H
#define MEDIA_ELEMENT_STATE_H

#ifdef MEDIA_HTML_SUPPORT

/** HTMLMediaElement.preload. */
class MediaPreload
{
public:
	enum State
	{
		NONE,
		METADATA,
		INVOKED,
		AUTO
	};

	MediaPreload() { /* uninitialized */ }
	MediaPreload(State state) : state(state) {}
#ifdef _DEBUG
	BOOL operator==(const MediaPreload& other) const { return state == other.state; }
	BOOL operator!=(const MediaPreload& other) const { return state != other.state; }
	BOOL operator<(const MediaPreload& other) const { return state < other.state; }
	BOOL operator<=(const MediaPreload& other) const { return state <= other.state; }
	BOOL operator>(const MediaPreload& other) const { return state > other.state; }
	BOOL operator>=(const MediaPreload& other) const { return state >= other.state; }
#else
	MediaPreload(unsigned char state) : state((State)state) {}
	operator unsigned char() const { return state; }
#endif

	/** Convert attribute value to preload state. */
	explicit MediaPreload(const uni_char* attrval)
	{
		state = MEDIA_PRELOAD_DEFAULT;
		if (attrval)
		{
			if (uni_stricmp(attrval, "none") == 0)
				state = NONE;
			else if (uni_stricmp(attrval, "metadata") == 0)
				state = METADATA;
			else if (uni_stricmp(attrval, "") == 0 ||
					 uni_stricmp(attrval, "auto") == 0)
				state = AUTO;
		}
	}

	/** Convert preload state to attribute value. */
	const uni_char* DOMValue() const
	{
		switch(state)
		{
		case NONE:
			return UNI_L("none");
		case METADATA:
			return UNI_L("metadata");
		case AUTO:
			return UNI_L("auto");
		}
		OP_ASSERT(!"unserializable state");
		return NULL;
	}

#ifdef _DEBUG
	const char* CStr() const
	{
		switch(state)
		{
		case INVOKED: return "INVOKED";
		case NONE: return "NONE";
		case METADATA: return "METADATA";
		case AUTO: return "AUTO";
		}
		OP_ASSERT(FALSE);
		return NULL;
	}
#endif // _DEBUG

private:
	State state;
};

/** HTMLMediaElement.networkState. */
class MediaNetwork
{
public:
	enum State
	{
		EMPTY = 0,
		IDLE,
		LOADING,
		NO_SOURCE
	};

	MediaNetwork(State state) : state(state) {}
#ifdef _DEBUG
	BOOL operator==(const MediaNetwork& other) const { return state == other.state; }
	BOOL operator!=(const MediaNetwork& other) const { return state != other.state; }
#else
	MediaNetwork(unsigned char state) : state((State)state) {}
	operator unsigned char() const { return state; }
#endif

	unsigned short DOMValue() { return state; }

#ifdef _DEBUG
	const char* CStr() const
	{
		switch(state)
		{
		case EMPTY: return "EMPTY";
		case IDLE: return "IDLE";
		case LOADING: return "LOADING";
		case NO_SOURCE: return "NO_SOURCE";
		}
		OP_ASSERT(FALSE);
		return NULL;
	}
#endif // _DEBUG

private:
	State state;
};

/** HTMLMediaElement.readyState. */
class MediaReady
{
public:
	enum State
	{
		NOTHING = 0,
		METADATA,
		CURRENT_DATA,
		FUTURE_DATA,
		ENOUGH_DATA
	};

	MediaReady(State state) : state(state) {}
#ifdef _DEBUG
	BOOL operator==(const MediaReady& other) const { return state == other.state; }
	BOOL operator!=(const MediaReady& other) const { return state != other.state; }
	BOOL operator<(const MediaReady& other) const { return state < other.state; }
	BOOL operator<=(const MediaReady& other) const { return state <= other.state; }
	BOOL operator>(const MediaReady& other) const { return state > other.state; }
	BOOL operator>=(const MediaReady& other) const { return state >= other.state; }
	MediaReady& operator++() { OP_ASSERT(state < ENOUGH_DATA); state = (State)((int)state + 1); return *this; }
	MediaReady& operator--() { OP_ASSERT(state > NOTHING); state = (State)((int)state - 1); return *this; }
#else
	MediaReady(unsigned char state) : state((State)state) {}
	operator unsigned char() const { return state; }
#endif

	unsigned short DOMValue() { return state; }

#ifdef _DEBUG
	const char* CStr() const
	{
		switch(state)
		{
		case NOTHING: return "NOTHING";
		case METADATA: return "METADATA";
		case CURRENT_DATA: return "CURRENT_DATA";
		case FUTURE_DATA: return "FUTURE_DATA";
		case ENOUGH_DATA: return "ENOUGH_DATA";
		}
		OP_ASSERT(FALSE);
		return NULL;
	}
#endif // _DEBUG

private:
	State state;
};

/** MediaState encapsulates preload, networkState and readyState.
 *
 * Not all combinations of the 3 states make sense, so what is
 * reported via the getters is coerced in certain ways. Some changes
 * to made via the setters result in no transitions, while others can
 * result in multiple transitions.
 */
class MediaState
{
public:
	MediaPreload GetPreload() const { return coerced_preload; }
	void SetPreload(MediaPreload state)
	{
		OP_NEW_DBG("SetPreload", "MediaState");
		OP_DBG((state.CStr()));
		current_preload = state;
	}
	void ResetPreload()
	{
		OP_NEW_DBG("ResetPreload", "MediaState");
		OP_DBG((""));
		coerced_preload = current_preload = MediaPreload::NONE;
	}

	MediaNetwork GetNetwork() const { return coerced_network; }
	void SetNetwork(MediaNetwork state)
	{
		OP_NEW_DBG("SetNetwork", "MediaState");
		OP_DBG((state.CStr()));
		current_network = state;
	}

	MediaReady GetReady() const { return coerced_ready; }
	void SetReady(MediaReady state)
	{
		OP_NEW_DBG("SetReady", "MediaState");
		OP_DBG((state.CStr()));
		current_ready = state;
	}

	void SetPendingReady(MediaReady state)
	{
		OP_NEW_DBG("SetPendingReady", "MediaState");
		OP_DBG((state.CStr()));
		pending_ready = state;
	}

	BOOL Transition();

	MediaState() :
		coerced_preload(MediaPreload::NONE),
		coerced_network(MediaNetwork::EMPTY),
		coerced_ready(MediaReady::NOTHING),
		current_preload(MediaPreload::NONE),
		current_network(MediaNetwork::EMPTY),
		current_ready(MediaReady::NOTHING),
		pending_ready(MediaReady::NOTHING) {}

#ifdef SELFTEST
	MediaState(MediaPreload coerced_preload, MediaNetwork coerced_network, MediaReady coerced_ready,
			   MediaPreload current_preload, MediaNetwork current_network, MediaReady current_ready,
			   MediaReady pending_ready) :
		coerced_preload(coerced_preload),
		coerced_network(coerced_network),
		coerced_ready(coerced_ready),
		current_preload(current_preload),
		current_network(current_network),
		current_ready(current_ready),
		pending_ready(pending_ready) {}
#endif // SELFTEST

#ifdef _DEBUG
	/** coerced state */
	MediaPreload coerced_preload;
	MediaNetwork coerced_network;
	MediaReady coerced_ready;

	/** current state */
	MediaPreload current_preload;
	MediaNetwork current_network;
	MediaReady current_ready;

	/** pending state */
	MediaReady pending_ready;
#else
	unsigned char coerced_preload:2; // 4 states
	unsigned char coerced_network:2; // 4 states
	unsigned char coerced_ready:3;   // 5 states
	unsigned char current_preload:2; // 4 states
	unsigned char current_network:2; // 4 states
	unsigned char current_ready:3;   // 5 states
	unsigned char pending_ready:3;   // 5 states
#endif
};

#endif // MEDIA_HTML_SUPPORT

#endif // MEDIA_ELEMENT_STATE_H
