/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/media/domtexttrack.h"

/* static */ OP_STATUS
DOM_TextTrack::Make(DOM_TextTrack*& domtrack, DOM_Environment* environment, MediaTrack* track)
{
	DOM_Runtime *runtime = static_cast<DOM_EnvironmentImpl*>(environment)->GetDOMRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(domtrack = OP_NEW(DOM_TextTrack, ()), runtime, runtime->GetPrototype(DOM_Runtime::TEXTTRACK_PROTOTYPE), "TextTrack"));

	domtrack->m_track = track;
	return OpStatus::OK;
}

DOM_TextTrack::~DOM_TextTrack()
{
	MediaDOMItem::DetachOrDestroy(m_track);
}

ES_GetState
DOM_TextTrack::GetCueList(MediaTrackCueList* cuelist, ES_Value* value, DOM_TextTrackCueList*& domcuelist)
{
	if (value)
	{
		DOMSetNull(value);

		if (m_track->GetMode() != TRACK_MODE_DISABLED)
		{
			if (!cuelist->GetDOMObject())
				GET_FAILED_IF_ERROR(DOM_TextTrackCueList::Make(domcuelist, GetEnvironment(), cuelist));

			DOMSetObject(value, domcuelist);
		}
	}
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_TextTrack::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_kind:
		DOMSetString(value, m_track->GetKindAsString());
		return GET_SUCCESS;

	case OP_ATOM_label:
		DOMSetString(value, m_track->GetLabelAsString());
		return GET_SUCCESS;

	case OP_ATOM_language:
		DOMSetString(value, m_track->GetLanguage());
		return GET_SUCCESS;

	case OP_ATOM_cues:
		return GetCueList(m_track->GetCueList(), value, m_cues);

	case OP_ATOM_activeCues:
		return GetCueList(m_track->GetActiveCueList(), value, m_active_cues);

	case OP_ATOM_oncuechange:
		return GetEventProperty(UNI_L("oncuechange"), value,
								static_cast<DOM_Runtime *>(origining_runtime));

	case OP_ATOM_mode:
		DOMSetString(value, m_track->DOMGetMode());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TextTrack::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_PutState state = DOM_Object::PutName(property_name, property_code, value, origining_runtime);
	if (state == PUT_FAILED && !GetIsSignificant())
	{
		SetIsSignificant();
		return PUT_FAILED_DONT_CACHE;
	}
	return state;
}

/* virtual */ ES_PutState
DOM_TextTrack::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_kind:
	case OP_ATOM_label:
	case OP_ATOM_language:
	case OP_ATOM_cues:
	case OP_ATOM_activeCues:
		return PUT_READ_ONLY;

	case OP_ATOM_oncuechange:
		return PutEventProperty(UNI_L("oncuechange"), value,
								static_cast<DOM_Runtime *>(origining_runtime));

	case OP_ATOM_mode:
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			DOM_EnvironmentImpl* environment = GetEnvironment();
			DOM_EnvironmentImpl::CurrentState state(environment, static_cast<DOM_Runtime*>(origining_runtime));

			m_track->DOMSetMode(environment, value->value.string, value->GetStringLength());
			return PUT_SUCCESS;
		}
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_TextTrack::GCTrace()
{
	MediaTrackCueList* cuelist = m_track->GetCueList();
	unsigned num_cues = cuelist->GetLength();
	for (unsigned cue_idx = 0; cue_idx < num_cues; ++cue_idx)
	{
		MediaTrackCue* cue = cuelist->GetItem(cue_idx);
		DOM_Object* dom_o = cue->GetDOMObject();
		if (dom_o && static_cast<DOM_TextTrackCue*>(dom_o)->GetIsSignificant())
			GCMark(dom_o);
	}

	GCMark(m_cues);
	GCMark(m_active_cues);

	GCMark(event_target);
}

/* virtual */ OP_STATUS
DOM_TextTrack::CreateEventTarget()
{
	SetIsSignificant();

	return DOM_EventTargetOwner::CreateEventTarget();
}

/* static */ int
DOM_TextTrack::addOrRemoveCue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(domtrack, DOM_TYPE_TEXTTRACK, DOM_TextTrack);

	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(domcue, 0, DOM_TYPE_TEXTTRACKCUE, DOM_TextTrackCue);

	MediaTrack* track = domtrack->m_track;
	MediaTrackCue* cue = domcue->GetCue();
	MediaTrack* cue_track = cue->GetOwnerTrack();

	DOM_EnvironmentImpl* environment = domtrack->GetEnvironment();
	DOM_EnvironmentImpl::CurrentState state(environment, origining_runtime);

	if (data == 0)
	{
		// addCue
		if (cue_track != NULL)
			// The cue is in a list already (has an owning
			// track). Remove it from it's current list first.
			cue_track->DOMRemoveCue(environment, cue);

		CALL_FAILED_IF_ERROR(track->DOMAddCue(environment, cue, domcue));

		if (domcue->GetIsSignificant())
			domtrack->SetIsSignificant();
	}
	else
	{
		// removeCue
		if (cue_track != track)
			return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);

		track->DOMRemoveCue(environment, cue);

		domtrack->SetIsSignificant();
	}
	return ES_FAILED;
}

DOM_FUNCTIONS_START(DOM_TextTrack)
	DOM_FUNCTIONS_FUNCTION(DOM_TextTrack, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_TextTrack)

DOM_FUNCTIONS_WITH_DATA_START(DOM_TextTrack)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrack, DOM_TextTrack::addOrRemoveCue, 0, "addCue", NULL)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrack, DOM_TextTrack::addOrRemoveCue, 1, "removeCue", NULL)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrack, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrack, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_TextTrack)

/* static */ OP_STATUS
DOM_TextTrackList::Make(DOM_TextTrackList*& domtracklist, DOM_Environment* environment,
						MediaTrackList* tracklist)
{
	if (!tracklist)
		return OpStatus::ERR_NO_MEMORY;

	DOM_Runtime *runtime = static_cast<DOM_EnvironmentImpl*>(environment)->GetDOMRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(domtracklist = OP_NEW(DOM_TextTrackList, ()), runtime, runtime->GetPrototype(DOM_Runtime::TEXTTRACKLIST_PROTOTYPE), "TextTrackList"));

	domtracklist->m_tracklist = tracklist;
	tracklist->SetDOMObject(domtracklist);
	return OpStatus::OK;
}

DOM_TextTrackList::~DOM_TextTrackList()
{
	MediaDOMItem::DetachOrDestroy(m_tracklist);
}

/* virtual */ ES_GetState
DOM_TextTrackList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	unsigned idx = static_cast<unsigned>(property_index);
	if (idx >= m_tracklist->GetLength())
		return GET_FAILED;

	MediaTrack* track = m_tracklist->GetTrackAt(idx);
	DOM_Object* domtrack = track->GetDOMObject();
	if (!domtrack)
	{
		DOM_TextTrack* domtexttrack;
		GET_FAILED_IF_ERROR(DOM_TextTrack::Make(domtexttrack, GetEnvironment(), track));

		track->SetDOMObject(domtexttrack);
		domtrack = domtexttrack;
	}

	DOMSetObject(value, domtrack);
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_TextTrackList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_READ_ONLY;
}

/* virtual */ OP_STATUS
DOM_TextTrackList::CountIndexedProperties(int &count, ES_Runtime *origining_runtime)
{
	count = m_tracklist->GetLength();
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TextTrackList::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_tracklist->GetLength());
		return GET_SUCCESS;
	}

	if (property_name == OP_ATOM_onaddtrack || property_name == OP_ATOM_onremovetrack)
		return GetEventProperty(property_name == OP_ATOM_onaddtrack ? UNI_L("onaddtrack") : UNI_L("onremovetrack"),
								value, static_cast<DOM_Runtime *>(origining_runtime));

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TextTrackList::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	if (property_name == OP_ATOM_onaddtrack || property_name == OP_ATOM_onremovetrack)
		return PutEventProperty(property_name == OP_ATOM_onaddtrack ? UNI_L("onaddtrack") : UNI_L("onremovetrack"),
								value, static_cast<DOM_Runtime *>(origining_runtime));

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_TextTrackList::GCTrace()
{
	unsigned num_tracks = m_tracklist->GetLength();
	for (unsigned track_idx = 0; track_idx < num_tracks; ++track_idx)
	{
		MediaTrack* track = m_tracklist->GetTrackAt(track_idx);
		DOM_Object* dom_o = track->GetDOMObject();
		if (dom_o && static_cast<DOM_TextTrack*>(dom_o)->GetIsSignificant())
			GCMark(dom_o);
	}

	GCMark(event_target);
}

DOM_FUNCTIONS_START(DOM_TextTrackList)
	DOM_FUNCTIONS_FUNCTION(DOM_TextTrackList, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_TextTrackList)

DOM_FUNCTIONS_WITH_DATA_START(DOM_TextTrackList)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrackList, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrackList, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_TextTrackList)

/* static */ OP_STATUS
DOM_TextTrackCue::Make(DOM_TextTrackCue*& domcue, DOM_Environment* environment,
					   MediaTrackCue* cue)
{
	DOM_Runtime *runtime = static_cast<DOM_EnvironmentImpl*>(environment)->GetDOMRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(domcue = OP_NEW(DOM_TextTrackCue, ()), runtime, runtime->GetPrototype(DOM_Runtime::TEXTTRACKCUE_PROTOTYPE), "TextTrackCue"));

	domcue->m_cue = cue;
	return OpStatus::OK;
}

DOM_TextTrackCue::~DOM_TextTrackCue()
{
	UnassociateCue();

	MediaDOMItem::DetachOrDestroy(m_cue);
}

/* virtual */ ES_GetState
DOM_TextTrackCue::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_track:
		if (value)
		{
			if (MediaTrack* track = m_cue->GetOwnerTrack())
			{
				DOM_Object* domtrack = track->GetDOMObject();
				if (!domtrack)
				{
					DOM_TextTrack* domtexttrack;
					GET_FAILED_IF_ERROR(DOM_TextTrack::Make(domtexttrack, GetEnvironment(), track));

					track->SetDOMObject(domtexttrack);
					domtrack = domtexttrack;
				}
				DOMSetObject(value, domtrack);
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_id:
		{
			const StringWithLength& id = m_cue->GetIdentifier();
			DOMSetStringWithLength(value, &(g_DOM_globalData->string_with_length_holder), id.string, id.length);
			return GET_SUCCESS;
		}

	case OP_ATOM_startTime:
		DOMSetNumber(value, m_cue->GetStartTime());
		return GET_SUCCESS;

	case OP_ATOM_endTime:
		DOMSetNumber(value, m_cue->GetEndTime());
		return GET_SUCCESS;

	case OP_ATOM_pauseOnExit:
		DOMSetBoolean(value, m_cue->GetPauseOnExit());
		return GET_SUCCESS;

	case OP_ATOM_snapToLines:
		DOMSetBoolean(value, m_cue->GetSnapToLines());
		return GET_SUCCESS;

	case OP_ATOM_line:
		if (value)
			DOMSetNumber(value, m_cue->DOMGetLinePosition());

		return GET_SUCCESS;

	case OP_ATOM_position:
		DOMSetNumber(value, m_cue->GetTextPosition());
		return GET_SUCCESS;

	case OP_ATOM_size:
		DOMSetNumber(value, m_cue->GetSize());
		return GET_SUCCESS;

	case OP_ATOM_align:
		{
			MediaTrackCueAlignment align = m_cue->GetAlignment();

			const uni_char* align_str = NULL;
			switch (align)
			{
			default:
			case CUE_ALIGNMENT_START:
				align_str = UNI_L("start");
				break;
			case CUE_ALIGNMENT_MIDDLE:
				align_str = UNI_L("middle");
				break;
			case CUE_ALIGNMENT_END:
				align_str = UNI_L("end");
				break;
			}
			DOMSetString(value, align_str);
		}
		return GET_SUCCESS;

	case OP_ATOM_text:
		{
			const StringWithLength& text = m_cue->GetText();
			DOMSetStringWithLength(value, &(g_DOM_globalData->string_with_length_holder), text.string, text.length);
			return GET_SUCCESS;
		}

	case OP_ATOM_onenter:
	case OP_ATOM_onexit:
		return GetEventProperty(property_name == OP_ATOM_onenter ? UNI_L("onenter") : UNI_L("onexit"),
								value, static_cast<DOM_Runtime *>(origining_runtime));
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TextTrackCue::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_PutState state = DOM_Object::PutName(property_name, property_code, value, origining_runtime);
	if (state == PUT_FAILED && !GetIsSignificant())
	{
		SetIsSignificant();
		return PUT_FAILED_DONT_CACHE;
	}
	return state;
}

/* virtual */ ES_PutState
DOM_TextTrackCue::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	DOM_EnvironmentImpl* environment = GetEnvironment();
	DOM_EnvironmentImpl::CurrentState state(environment, static_cast<DOM_Runtime*>(origining_runtime));

	switch (property_name)
	{
	case OP_ATOM_track:
		return PUT_READ_ONLY;

	case OP_ATOM_id:
		if (value->type != VALUE_STRING_WITH_LENGTH)
			return PUT_NEEDS_STRING_WITH_LENGTH;
		else
		{
			StringWithLength id(*value->value.string_with_length);
			PUT_FAILED_IF_ERROR(m_cue->SetIdentifier(id));
			return PUT_SUCCESS;
		}

	case OP_ATOM_startTime:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;

		m_cue->DOMSetStartTime(environment, value->value.number);
		return PUT_SUCCESS;

	case OP_ATOM_endTime:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;

		m_cue->DOMSetEndTime(environment, value->value.number);
		return PUT_SUCCESS;

	case OP_ATOM_pauseOnExit:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

		m_cue->SetPauseOnExit(!!value->value.boolean);
		return PUT_SUCCESS;

	case OP_ATOM_snapToLines:
		{
			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;

			int line_position = m_cue->GetLinePosition();
			if (!value->value.boolean && (line_position < 0 || line_position > 100))
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);

			m_cue->DOMSetSnapToLines(environment, !!value->value.boolean);
		}
		return PUT_SUCCESS;

	case OP_ATOM_line:
		{
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;

			int line_position = TruncateDoubleToInt(value->value.number);
			if (!m_cue->GetSnapToLines() && (line_position < 0 || line_position > 100))
				return DOM_PUTNAME_DOMEXCEPTION(INDEX_SIZE_ERR);

			m_cue->DOMSetLinePosition(environment, line_position);
		}
		return PUT_SUCCESS;

	case OP_ATOM_position:
	case OP_ATOM_size:
		{
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;

			int num_value = TruncateDoubleToInt(value->value.number);
			if (num_value < 0 || num_value > 100)
				return DOM_PUTNAME_DOMEXCEPTION(INDEX_SIZE_ERR);

			if (property_name == OP_ATOM_position)
				m_cue->DOMSetTextPosition(environment, num_value);
			else
				m_cue->DOMSetSize(environment, num_value);
		}
		return PUT_SUCCESS;

	case OP_ATOM_align:
		{
			if (value->type != VALUE_STRING_WITH_LENGTH)
				return PUT_NEEDS_STRING_WITH_LENGTH;

			const uni_char* str = value->value.string_with_length->string;
			unsigned str_length = value->value.string_with_length->length;

			MediaTrackCueAlignment alignment;
			if (str_length == 5 && uni_strncmp(str, "start", 5) == 0)
				alignment = CUE_ALIGNMENT_START;
			else if (str_length == 6 && uni_strncmp(str, "middle", 6) == 0)
				alignment = CUE_ALIGNMENT_MIDDLE;
			else if (str_length == 3 && uni_strncmp(str, "end", 3) == 0)
				alignment = CUE_ALIGNMENT_END;
			else
				return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);

			m_cue->DOMSetAlignment(environment, alignment);
		}
		return PUT_SUCCESS;

	case OP_ATOM_text:
		if (value->type != VALUE_STRING_WITH_LENGTH)
			return PUT_NEEDS_STRING_WITH_LENGTH;
		else
		{
			StringWithLength text(*value->value.string_with_length);
			PUT_FAILED_IF_ERROR(m_cue->DOMSetText(environment, text));
			return PUT_SUCCESS;
		}

	case OP_ATOM_onenter:
	case OP_ATOM_onexit:
		return PutEventProperty(property_name == OP_ATOM_onenter ? UNI_L("onenter") : UNI_L("onexit"),
								value, static_cast<DOM_Runtime *>(origining_runtime));
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ OP_STATUS
DOM_TextTrackCue::CreateEventTarget()
{
	SetIsSignificant();

	return DOM_EventTargetOwner::CreateEventTarget();
}

/* virtual */ void
DOM_TextTrackCue::GCTrace()
{
	GCMark(event_target);
}

void
DOM_TextTrackCue::SetIsSignificant()
{
	m_is_significant = 1;

	// Mark the owning track as significant - if any.
	if (MediaTrack* track = m_cue->GetOwnerTrack())
		if (DOM_Object* domtrack = track->GetDOMObject())
			static_cast<DOM_TextTrack*>(domtrack)->SetIsSignificant();
}

DOM_Document*
DOM_TextTrackCue::GetOwnerDocument()
{
	/* Find the media element a cue belongs to, and return its
	   owner document. */
	if (MediaTrack* cue_track = m_cue->GetOwnerTrack())
		if (HTML_Element* media_element = cue_track->GetAssociatedHtmlElement())
			if (DOM_Object* dom_media_element = media_element->GetESElement())
				return static_cast<DOM_Node*>(dom_media_element)->GetOwnerDocument();

	return NULL;
}

/* static */ int
DOM_TextTrackCue::getCueAsHTML(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcue, DOM_TYPE_TEXTTRACKCUE, DOM_TextTrackCue);

	DOMSetNull(return_value);

	/* Spec. says:

	   "The getCueAsHTML() method must convert the text track cue text
	   to a DocumentFragment for the media element's Document"

	   Try to acquire a reference to that document. */
	if (DOM_Document* cue_owner_doc = domcue->GetOwnerDocument())
	{
		DOM_DocumentFragment* document_fragment;
		CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(document_fragment, cue_owner_doc));

		HTML_Element* doc_frag_root = document_fragment->GetPlaceholderElement();
		HLDocProfile* hld_profile = cue_owner_doc->GetHLDocProfile();

		CALL_FAILED_IF_ERROR(domcue->m_cue->DOMGetAsHTML(hld_profile, doc_frag_root));

		DOMSetObject(return_value, document_fragment);
	}
	return ES_VALUE;
}

DOM_FUNCTIONS_START(DOM_TextTrackCue)
	DOM_FUNCTIONS_FUNCTION(DOM_TextTrackCue, DOM_TextTrackCue::getCueAsHTML, "getCueAsHTML", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_TextTrackCue, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_TextTrackCue)

DOM_FUNCTIONS_WITH_DATA_START(DOM_TextTrackCue)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrackCue, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TextTrackCue, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_TextTrackCue)

/* virtual */ int
DOM_TextTrackCue_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("nnz");

	MediaTrackCue* cue;
	CALL_FAILED_IF_ERROR(MediaTrackCue::DOMCreate(cue,
												  argv[0].value.number /* startTime */,
												  argv[1].value.number /* endTime */,
												  StringWithLength(*argv[2].value.string_with_length) /* text */));

	DOM_TextTrackCue* domcue;
	OP_STATUS status = DOM_TextTrackCue::Make(domcue, GetEnvironment(), cue);
	if (OpStatus::IsError(status))
		OP_DELETE(cue);

	CALL_FAILED_IF_ERROR(status);

	DOMSetObject(return_value, domcue);
	return ES_VALUE;
}

/* static */ OP_STATUS
DOM_TextTrackCueList::Make(DOM_TextTrackCueList*& domcuelist, DOM_Environment* environment,
						   MediaTrackCueList* cuelist)
{
	DOM_Runtime *runtime = static_cast<DOM_EnvironmentImpl*>(environment)->GetDOMRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(domcuelist = OP_NEW(DOM_TextTrackCueList, ()), runtime, runtime->GetPrototype(DOM_Runtime::TEXTTRACKCUELIST_PROTOTYPE), "TextTrackCueList"));

	domcuelist->m_cuelist = cuelist;
	cuelist->SetDOMObject(domcuelist);
	return OpStatus::OK;
}

DOM_TextTrackCueList::~DOM_TextTrackCueList()
{
	MediaDOMItem::DetachOrDestroy(m_cuelist);
}

OP_STATUS DOM_TextTrackCueList::GetDOMItem(MediaTrackCue* cue, DOM_Object*& domcue)
{
	domcue = cue->GetDOMObject();
	if (!domcue)
	{
		DOM_TextTrackCue* domtextcue;
		RETURN_IF_ERROR(DOM_TextTrackCue::Make(domtextcue, GetEnvironment(), cue));

		cue->SetDOMObject(domtextcue);
		domcue = domtextcue;
	}
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TextTrackCueList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	unsigned idx = static_cast<unsigned>(property_index);
	if (idx >= m_cuelist->GetLength())
		return GET_FAILED;

	DOM_Object* domcue;
	GET_FAILED_IF_ERROR(GetDOMItem(m_cuelist->GetItem(idx), domcue));

	DOMSetObject(value, domcue);
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_TextTrackCueList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_READ_ONLY;
}

/* virtual */ OP_STATUS
DOM_TextTrackCueList::CountIndexedProperties(int &count, ES_Runtime *origining_runtime)
{
	count = m_cuelist->GetLength();
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TextTrackCueList::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_cuelist->GetLength());
		return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TextTrackCueList::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* static */ int
DOM_TextTrackCueList::getCueById(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcuelist, DOM_TYPE_TEXTTRACKCUELIST, DOM_TextTrackCueList);
	DOM_CHECK_ARGUMENTS("z");

	DOMSetNull(return_value);

	ES_ValueString* id_needle = argv[0].value.string_with_length;
	if (MediaTrackCue* cue = domcuelist->m_cuelist->GetCueById(StringWithLength(id_needle->string, id_needle->length)))
	{
		DOM_Object* domcue;
		CALL_FAILED_IF_ERROR(domcuelist->GetDOMItem(cue, domcue));

		DOMSetObject(return_value, domcue);
	}
	return ES_VALUE;
}

DOM_FUNCTIONS_START(DOM_TextTrackCueList)
	DOM_FUNCTIONS_FUNCTION(DOM_TextTrackCueList, DOM_TextTrackCueList::getCueById, "getCueById", "z-")
DOM_FUNCTIONS_END(DOM_TextTrackCueList)

#endif // MEDIA_HTML_SUPPORT
