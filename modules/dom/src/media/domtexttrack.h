/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TEXTTRACK_H
#define DOM_TEXTTRACK_H

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/media/mediatrack.h"

class DOM_TextTrackCueList;

class DOM_TextTrack
	: public DOM_Object,
	  public DOM_EventTargetOwner
{
private:
	DOM_TextTrack() :
		m_track(NULL),
		m_cues(NULL),
		m_active_cues(NULL),
		m_is_significant(0) {}
	virtual ~DOM_TextTrack();

	MediaTrack* m_track;

	DOM_TextTrackCueList* m_cues;
	DOM_TextTrackCueList* m_active_cues;

	ES_GetState GetCueList(MediaTrackCueList* cuelist, ES_Value* value, DOM_TextTrackCueList*& domcuelist);

	unsigned m_is_significant:1;
	/**< Objects with this flag set should be kept alive until the
	   underlying track object is destroyed. If the flag is not set,
	   the DOM object can be garbage collected early to save
	   memory. This is a mechanism similar to the one employed by
	   DOM_Node. */

public:
	static OP_STATUS Make(DOM_TextTrack*& domtrack, DOM_Environment* environment, MediaTrack* track);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TEXTTRACK || DOM_Object::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void GCTrace();

	DOM_DECLARE_FUNCTION_WITH_DATA(addOrRemoveCue);

	// From DOM_EventTargetOwner
	virtual DOM_Object* GetOwnerObject() { return this; }
	virtual OP_STATUS CreateEventTarget();

	void SetIsSignificant() { m_is_significant = 1; }
	BOOL GetIsSignificant() { return (m_is_significant == 1); }

	enum
	{
		// EventTarget
		FUNCTIONS_dispatchEvent = 1,
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		FUNCTIONS_WITH_DATA_addCue = 1,
		FUNCTIONS_WITH_DATA_removeCue,

		// EventTarget
		FUNCTIONS_WITH_DATA_addEventListener,
		FUNCTIONS_WITH_DATA_removeEventListener,

		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

class DOM_TextTrackList
	: public DOM_Object,
	  public DOM_EventTargetOwner
{
private:
	DOM_TextTrackList() : m_tracklist(NULL) {}
	virtual ~DOM_TextTrackList();

	MediaTrackList* m_tracklist;

public:
	static OP_STATUS Make(DOM_TextTrackList*& domtracklist, DOM_Environment* environment,
						  MediaTrackList* tracklist);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TEXTTRACKLIST || DOM_Object::IsA(type); }

	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual OP_STATUS CountIndexedProperties(int &count, ES_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void GCTrace();

	// From DOM_EventTargetOwner
	virtual DOM_Object* GetOwnerObject() { return this; }

	enum
	{
		// EventTarget
		FUNCTIONS_dispatchEvent = 1,
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		// EventTarget
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

class DOM_TextTrackCue
	: public DOM_Object,
	  public DOM_EventTargetOwner
{
private:
	DOM_TextTrackCue() : m_cue(NULL), m_is_significant(0) {}
	virtual ~DOM_TextTrackCue();

	MediaTrackCue* m_cue;

	unsigned m_is_significant:1;
	/**< See DOM_TextTrack::m_is_significant. */

	DOM_Document* GetOwnerDocument();

	void SetIsSignificant();

	void UnassociateCue()
	{
		// If the cue has no owner track it is either weakly
		// associated with a track or a standalone cue. In the former
		// case we need to remove the association.
		if (m_cue->GetOwnerTrack() == NULL)
			m_cue->Out();
	}

public:
	static OP_STATUS Make(DOM_TextTrackCue*& domcue, DOM_Environment* environment,
						  MediaTrackCue* cue);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TEXTTRACKCUE || DOM_Object::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void GCTrace();

	// From DOM_EventTargetOwner
	virtual DOM_Object* GetOwnerObject() { return this; }
	virtual OP_STATUS CreateEventTarget();

	DOM_DECLARE_FUNCTION(getCueAsHTML);

	MediaTrackCue* GetCue() { return m_cue; }

	BOOL GetIsSignificant() { return (m_is_significant == 1); }

	enum
	{
		FUNCTIONS_getCueAsHTML = 1,
		// EventTarget
		FUNCTIONS_dispatchEvent,
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		// EventTarget
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

class DOM_TextTrackCue_Constructor :
	public DOM_BuiltInConstructor
{
public:
	DOM_TextTrackCue_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::TEXTTRACKCUE_PROTOTYPE) {}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_TextTrackCueList
	: public DOM_Object
{
private:
	DOM_TextTrackCueList() : m_cuelist(NULL) {}
	virtual ~DOM_TextTrackCueList();

	MediaTrackCueList* m_cuelist;

	OP_STATUS GetDOMItem(MediaTrackCue* cue, DOM_Object*& domcue);

public:
	static OP_STATUS Make(DOM_TextTrackCueList*& domcuelist, DOM_Environment* environment,
						  MediaTrackCueList* cuelist);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TEXTTRACKCUELIST || DOM_Object::IsA(type); }

	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual OP_STATUS CountIndexedProperties(int &count, ES_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(getCueById);

	enum
	{
		FUNCTIONS_getCueById = 1,
		FUNCTIONS_ARRAY_SIZE
	};
};

#endif // MEDIA_HTML_SUPPORT
#endif // DOM_TEXTTRACK_H
