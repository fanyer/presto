/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/logdoc/logdoc_util.h"

DOM_Text::DOM_Text(DOM_NodeType type)
	: DOM_CharacterData(type)
{
}

/* static */ OP_STATUS
DOM_Text::Make(DOM_Text *&text, DOM_Node *reference, const uni_char *contents)
{
	if (!contents)
		contents = UNI_L("");

	DOM_CharacterData *characterdata;

	RETURN_IF_ERROR(DOM_CharacterData::Make(characterdata, reference, contents, FALSE, FALSE));
	OP_ASSERT(characterdata->IsA(DOM_TYPE_TEXT));

	text = (DOM_Text *) characterdata;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_Text::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
		DOMSetString(value, UNI_L("#text"));
		return GET_SUCCESS;

	case OP_ATOM_wholeText:
		return GetWholeText(value);

	case OP_ATOM_isElementContentWhitespace:
		if (value)
		{
			HTML_Element *element = GetThisElement();
			DOMSetBoolean(value, element->HasWhiteSpaceOnly());
		}
		return GET_SUCCESS;

	default:
		return DOM_CharacterData::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_Text::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_wholeText)
		return PUT_SUCCESS;
	else
		return DOM_CharacterData::PutName(property_name, value, origining_runtime);
}

OP_STATUS
DOM_Text::IsWhitespace(BOOL &is_whitespace)
{
	TempBuffer value;

	RETURN_IF_ERROR(GetValue(&value));

	if (owner_document->IsXML())
		is_whitespace = XMLUtils::IsWhitespace(value.GetStorage(), value.Length());
	else
		is_whitespace = WhiteSpaceOnly(value.GetStorage(), value.Length());

	return OpStatus::OK;
}

ES_GetState
DOM_Text::GetWholeText(ES_Value *value)
{
	if (value)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();
		TempBuffer *buffer = GetEmptyTempBuf();

		HTML_Element *element = GetThisElement();

		while (HTML_Element *previous_sibling = element->PredActual())
			if (previous_sibling->IsText())
				element = previous_sibling;
			else
				break;

		while (element && element->IsText())
		{
			GET_FAILED_IF_ERROR(element->DOMGetContents(environment, buffer));
			element = element->SucActual();
		}

		DOMSetString(value, buffer);
	}

	return GET_SUCCESS;
}

class DOM_SplitTextState
	: public DOM_Object
{
public:
	enum Point { CREATE_SPLIT, CHANGE_VALUE, FINISHED } point;
	DOM_Text *text;
	DOM_CharacterData *split;
	unsigned offset;
	ES_Value return_value;

	DOM_SplitTextState(Point point, DOM_Text *text, DOM_CharacterData *split, unsigned offset)
		: point(point), text(text), split(split), offset(offset)
	{
	}

	virtual void GCTrace()
	{
		GCMark(text);
		GCMark(split);
		GCMark(return_value);
	}
};

/* static */ int
DOM_Text::splitText(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_SplitTextState *state = NULL;
	DOM_SplitTextState::Point point = DOM_SplitTextState::CREATE_SPLIT;

#define IS_POINT(p) (point == DOM_SplitTextState::p)
#define SET_POINT(p) do { point = DOM_SplitTextState::p; if (state) state->point = DOM_SplitTextState::p; } while (0)
#define IS_RESTARTED (state != NULL)

	DOM_Text *text;
	DOM_CharacterData *split;
	unsigned offset;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(text, DOM_TYPE_TEXT, DOM_Text);
		DOM_CHECK_ARGUMENTS("n");

		if (!text->OriginCheck(origining_runtime))
			return ES_EXCEPT_SECURITY;

		if (argv[0].value.number < 0)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		offset = TruncateDoubleToUInt(argv[0].value.number);
		split = NULL;
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_SplitTextState);
		point = state->point;

		this_object = text = state->text;
		split = state->split;
		offset = state->offset;

		*return_value = state->return_value;
	}

	TempBuffer buf;
	CALL_FAILED_IF_ERROR(text->GetValue(&buf));
	uni_char* value = buf.GetStorage();

	unsigned int length = buf.Length();
	if (offset > length)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	if (IS_POINT(CREATE_SPLIT))
	{
		int result = ES_VALUE;

		if (!IS_RESTARTED)
		{
			unsigned int count = length - offset;

			TempBuffer buf;
			CALL_FAILED_IF_ERROR(buf.Append(value + offset, count));

			const uni_char *storage = buf.GetStorage();
			if (!storage)
				storage = UNI_L("");

			CALL_FAILED_IF_ERROR(DOM_CharacterData::Make(split, text, storage, FALSE, text->GetNodeType() == CDATA_SECTION_NODE));

			DOM_Node *parentNode;
			CALL_FAILED_IF_ERROR(text->GetParentNode(parentNode));

			if (parentNode)
			{
				DOM_Node *nextSibling;
				CALL_FAILED_IF_ERROR(text->GetNextSibling(nextSibling));

				ES_Value arguments[2];
				DOMSetObject(&arguments[0], split);
				DOMSetObject(&arguments[1], nextSibling);

				result = DOM_Node::insertBefore(parentNode, arguments, 2, return_value, origining_runtime);
			}
			else
				DOMSetObject(return_value, split);
		}
		else
			result = DOM_Node::insertBefore(NULL, NULL, -1, return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;

		if (result != ES_VALUE)
			return result;

		SET_POINT(CHANGE_VALUE);
	}

	if (IS_POINT(CHANGE_VALUE))
	{
		TempBuffer buf;
		CALL_FAILED_IF_ERROR(buf.Append(value, offset));

		const uni_char *storage = buf.GetStorage();
		if (!storage)
			storage = UNI_L("");

		CALL_FAILED_IF_ERROR(text->SetValue(storage, origining_runtime, DOM_MutationListener::SPLITTING, offset, length - offset));

		SET_POINT(FINISHED);

		if (GetCurrentThread(origining_runtime) && GetCurrentThread(origining_runtime)->IsBlocked())
			goto suspend;
	}

	return ES_VALUE;

suspend:
	if (!state)
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_SplitTextState, (point, text, split, offset)), text->GetRuntime()));

	state->return_value = *return_value;

#undef IS_POINT
#undef SET_POINT
#undef IS_RESTARTED

	DOMSetObject(return_value, state);
	return ES_SUSPEND | ES_RESTART;
}

class DOM_ReplaceWholeTextState
	: public DOM_Object
{
public:
	enum Point { REMOVE_ADJACENT, SET_TEXT } point;
	OpString replacementText;
	DOM_Node *iter, *current, *ancestor;
	ES_Value return_value;

	DOM_ReplaceWholeTextState(Point point, DOM_Node *iter, DOM_Node *current, DOM_Node *ancestor)
		: point(point), iter(iter), current(current), ancestor(ancestor)
	{
	}

	virtual void GCTrace()
	{
		GCMark(iter);
		GCMark(current);
		GCMark(return_value);
	}
};

/* static */ int
DOM_Text::replaceWholeText(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_ReplaceWholeTextState *state = NULL;
	DOM_ReplaceWholeTextState::Point point = DOM_ReplaceWholeTextState::REMOVE_ADJACENT;
	const uni_char *replacementText;
	DOM_Node *iter, *current, *ancestor;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(current, DOM_TYPE_TEXT, DOM_Node);
		DOM_CHECK_ARGUMENTS("s");

		replacementText = argv[0].value.string;
		if (!*replacementText)
			replacementText = NULL;

		HTML_Element *element = current->GetThisElement();
		while (HTML_Element *previous_sibling = element->PredActual())
			if (previous_sibling->IsText())
				element = previous_sibling;
			else
				break;

		CALL_FAILED_IF_ERROR(current->GetEnvironment()->ConstructNode(iter, element, current->GetOwnerDocument()));
		CALL_FAILED_IF_ERROR(current->GetParentNode(ancestor));
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_ReplaceWholeTextState);
		point = state->point;
		replacementText = state->replacementText.IsEmpty() ? NULL : state->replacementText.CStr();
		iter = state->iter;
		current = state->current;
		ancestor = state->ancestor;
		*return_value = state->return_value;

		if (point == DOM_ReplaceWholeTextState::REMOVE_ADJACENT)
		{
			DOM_Node *iter_parent = ancestor, *current_parent;
			if (iter)
				CALL_FAILED_IF_ERROR(iter->GetParentNode(iter_parent));
			CALL_FAILED_IF_ERROR(current->GetParentNode(current_parent));
			if (iter_parent != ancestor || current_parent != ancestor)
				return DOM_CALL_INTERNALEXCEPTION(UNEXPECTED_MUTATION_ERR);
		}

		int result;

		if (point == DOM_ReplaceWholeTextState::REMOVE_ADJACENT)
			result = DOM_Node::removeChild(ancestor, NULL, -1, return_value, origining_runtime);
		else
			result = DOM_CharacterData::replaceData(current, NULL, -1, return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;
		else if (result != ES_FAILED && result != ES_VALUE)
			return result;

		if (point == DOM_ReplaceWholeTextState::SET_TEXT)
		{
			DOMSetObject(return_value, current);
			return ES_VALUE;
		}

		if (point == DOM_ReplaceWholeTextState::REMOVE_ADJACENT && iter == NULL)
			point = DOM_ReplaceWholeTextState::SET_TEXT;
	}

	while (point == DOM_ReplaceWholeTextState::REMOVE_ADJACENT)
	{
		DOM_Node *to_remove;
		if (iter != current || !replacementText)
			to_remove = iter;
		else
			to_remove = NULL;

		HTML_Element *element = iter->GetThisElement(), *next_text = NULL;
		if (HTML_Element *next_sibling = element->SucActual())
			if (next_sibling->IsText())
				next_text = next_sibling;

		if (next_text)
			CALL_FAILED_IF_ERROR(current->GetEnvironment()->ConstructNode(iter, next_text, current->GetOwnerDocument()));
		else
			iter = NULL;

		if (to_remove)
		{
			ES_Value removeChild_argv[1];
			DOMSetObject(&removeChild_argv[0], to_remove);

			int result = DOM_Node::removeChild(ancestor, removeChild_argv, 1, return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;
			else if (result != ES_VALUE)
				return result;
		}

		if (iter == NULL)
			point = DOM_ReplaceWholeTextState::SET_TEXT;
	}

	if (replacementText)
	{
		TempBuffer value;
		CALL_FAILED_IF_ERROR(((DOM_CharacterData *) current)->GetValue(&value));

		ES_Value replaceData_argv[3];
		DOMSetNumber(&replaceData_argv[0], 0);
		DOMSetNumber(&replaceData_argv[1], value.Length());
		DOMSetString(&replaceData_argv[2], replacementText);

		int result = DOM_CharacterData::replaceData(current, replaceData_argv, 3, return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;
		else if (result != ES_FAILED)
			return result;

		DOMSetObject(return_value, current);
	}
	else
		DOMSetNull(return_value);

	return ES_VALUE;

suspend:
	if (!state)
	{
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_ReplaceWholeTextState, (point, iter, current, ancestor)), current->GetRuntime()));
		if (point == DOM_ReplaceWholeTextState::REMOVE_ADJACENT && replacementText)
			CALL_FAILED_IF_ERROR(state->replacementText.Set(replacementText));
	}

	state->return_value = *return_value;

	DOMSetObject(return_value, state);
	return ES_SUSPEND | ES_RESTART;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Text)
	DOM_FUNCTIONS_FUNCTION(DOM_Text, DOM_Text::splitText, "splitText", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_Text, DOM_Text::replaceWholeText, "replaceWholeText", "s-")
DOM_FUNCTIONS_END(DOM_Text)

