/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/util/simset.h"
#include "modules/widgets/UndoRedoStack.h"

#ifdef WIDGETS_UNDO_REDO_SUPPORT

//#define UNDO_EVERY_CHARACTER

// == UndoRedoEvent ==================================================

UndoRedoEvent* UndoRedoEvent::Construct(INT32 caret_pos, const uni_char* str, INT32 len)
{
	UndoRedoEvent* result = OP_NEW(UndoRedoEvent, ());
	if (result && OpStatus::IsSuccess(result->Create(caret_pos, str, len)))
		return result;
	
	OP_DELETE(result);
	return NULL;
}

UndoRedoEvent* UndoRedoEvent::Construct(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str)
{
	UndoRedoEvent* result = OP_NEW(UndoRedoEvent, ());
	if (result && OpStatus::IsSuccess(result->Create(caret_pos, sel_start, sel_stop, removed_str)))
		return result;
	
	OP_DELETE(result);
	return NULL;
}

UndoRedoEvent* UndoRedoEvent::ConstructReplace(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str, INT32 removed_str_length)
{
	UndoRedoEvent* result = OP_NEW(UndoRedoEvent, ());
	if (result && OpStatus::IsSuccess(result->CreateReplace(caret_pos, sel_start, sel_stop, removed_str, removed_str_length)))
		return result;
	
	OP_DELETE(result);
	return NULL;
}

#ifdef WIDGETS_IME_SUPPORT
UndoRedoEvent* UndoRedoEvent::ConstructEmpty()
{
	UndoRedoEvent* result = OP_NEW(UndoRedoEvent, ());
	if (result && OpStatus::IsSuccess(result->CreateEmpty()))
		return result;
	OP_DELETE(result);
	return NULL;
}
#endif // WIDGETS_IME_SUPPORT

UndoRedoEvent::UndoRedoEvent() :
	str(NULL),
	str_length(0),
	str_size(0),
	caret_pos(0),
	sel_start(-1),
	sel_stop(-1),
	event_type(EV_TYPE_INVALID),
	is_appended(FALSE)
{
}

OP_STATUS
UndoRedoEvent::Create(INT32 caret_pos, const uni_char* str, INT32 len)
{
	OP_ASSERT(str && *str);//insertion events must have some text

	this->str = UniSetNewStrN(str, RoundSizeUpper(len));
	RETURN_OOM_IF_NULL(this->str);

	this->str_length = len;
	this->str_size = RoundSizeUpper(len);
	this->caret_pos = caret_pos;
	this->event_type = EV_TYPE_INSERT;
	return OpStatus::OK;
}

OP_STATUS
UndoRedoEvent::Create(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str)
{
	OP_ASSERT(sel_start >= 0);
	OP_ASSERT(sel_stop >= 0);
	OP_ASSERT(removed_str && *removed_str);//remove events must have some text

	this->str_length = sel_stop - sel_start;
	this->str_size = RoundSizeUpper(this->str_length);
	this->str = UniSetNewStrN(removed_str, this->str_size);
	RETURN_OOM_IF_NULL(this->str);

	this->caret_pos = caret_pos;
	this->sel_start = sel_start;
	this->sel_stop = sel_stop;
	this->event_type = EV_TYPE_REMOVE;

	return OpStatus::OK;
}

OP_STATUS
UndoRedoEvent::CreateReplace(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str, INT32 removed_str_length)
{
	OP_ASSERT(sel_start >= 0);
	OP_ASSERT(sel_stop >= 0);
	OP_ASSERT(removed_str && *removed_str);//remove events must have some text

	this->str = UniSetNewStrN(removed_str, RoundSizeUpper(removed_str_length));
	RETURN_OOM_IF_NULL(this->str);

	this->str_length = removed_str_length;
	this->str_size = RoundSizeUpper(removed_str_length);
	this->caret_pos = caret_pos;
	this->sel_start = sel_start;
	this->sel_stop = sel_stop;
	this->event_type = EV_TYPE_REPLACE;

	return OpStatus::OK;
}

#ifdef WIDGETS_IME_SUPPORT
OP_STATUS UndoRedoEvent::CreateEmpty()
{
	this->str = 0;
	this->str_length = 0;
	this->str_size = 0;
	this->event_type = EV_TYPE_EMPTY;

	return OpStatus::OK;
}
#endif // WIDGETS_IME_SUPPORT

UndoRedoEvent::~UndoRedoEvent()
{
	Out();
	OP_DELETEA(str);
	str = NULL;
	str_length = 0;
	str_size = 0;
	event_type = EV_TYPE_INVALID;
}

UINT32 UndoRedoEvent::BytesUsed()
{
	return sizeof(UndoRedoEvent) + (str ? str_size + 1 : 0);
}

OP_STATUS UndoRedoEvent::Append(const uni_char *instr, INT32 inlen)
{
	if (str_size < str_length + inlen)
	{
		uni_char* newstr = OP_NEWA(uni_char, RoundSizeUpper(str_length + inlen) + 1);
		RETURN_OOM_IF_NULL(newstr);
		str_size = RoundSizeUpper(str_length + inlen);

		op_memcpy(newstr, str, UNICODE_SIZE(str_length));
		op_memcpy(&newstr[str_length], instr, UNICODE_SIZE(inlen));

		str_length += inlen;
		newstr[str_length] = 0;

		OP_DELETEA(str);
		str = newstr;
	}
	else
	{
		op_memcpy(&str[str_length], instr, UNICODE_SIZE(inlen));

		str_length += inlen;
		str[str_length] = 0;
	}
	is_appended = TRUE;

	return OpStatus::OK;
}

OP_STATUS UndoRedoEvent::AppendDeleted(const uni_char *instr, INT32 inlen)
{
	OP_ASSERT(caret_pos >= inlen);
	if (str_size < str_length + inlen)
	{
		uni_char* newstr = OP_NEWA(uni_char, RoundSizeUpper(str_length + inlen) + 1);
		RETURN_OOM_IF_NULL(newstr);
		str_size = RoundSizeUpper(str_length + inlen);

		op_memcpy(newstr, instr, UNICODE_SIZE(inlen));
		op_memcpy(&newstr[inlen], str, UNICODE_SIZE(str_length));

		str_length += inlen;
		newstr[str_length] = 0;

		OP_DELETEA(str);
		str = newstr;
	}
	else
	{
		//buffer overlaps -> use memmove
		op_memmove(&str[inlen], str, UNICODE_SIZE(str_length));
		op_memcpy(str, instr, UNICODE_SIZE(inlen));

		str_length += inlen;
		str[str_length] = 0;
	}
	is_appended = TRUE;

	if (sel_start == caret_pos)
		sel_start -= inlen;
	caret_pos -= inlen;

	return OpStatus::OK;
}

// == UndoRedoStack ==================================================

UndoRedoStack::UndoRedoStack()
	: mem_used(0)
{
}

UndoRedoStack::~UndoRedoStack()
{
	Clear();
}

void UndoRedoStack::Clear(BOOL clear_undos, BOOL clear_redos)
{
	UndoRedoEvent* event;

	// Clear undos
	if (clear_undos)
	{
		event = (UndoRedoEvent*) undos.First();
		while(event)
		{
			UndoRedoEvent* next_event = (UndoRedoEvent*) event->Suc();
			mem_used -= event->BytesUsed();
			OP_DELETE(event);
			event = next_event;
		}
	}
	// Clear redos
	if (clear_redos)
	{
		event = (UndoRedoEvent*) redos.First();
		while(event)
		{
			UndoRedoEvent* next_event = (UndoRedoEvent*) event->Suc();
			mem_used -= event->BytesUsed();
			OP_DELETE(event);
			event = next_event;
		}
	}
}

OP_STATUS UndoRedoStack::SubmitInsert(INT32 caret_pos, const uni_char* str, BOOL no_append, INT32 len)
{
	Clear(FALSE, TRUE);

	UndoRedoEvent* event;
#ifndef UNDO_EVERY_CHARACTER
	event = PeekUndo();
	if (event && !no_append)
	{
		//here we check if the user has only input one single char, so it
		//can be appended to the previous insert event
		if (event->GetType() == UndoRedoEvent::EV_TYPE_INSERT && len == 1)
		{
			OP_ASSERT(event->str && *event->str && event->str_length > 0);

			//this check causes events to be split on word break
			if ( !uni_isalnum(*str) || uni_isalnum(event->str[event->str_length-1]) )
		{
			if (event->caret_pos == caret_pos - 1 ||
						(event->is_appended && event->caret_pos + event->str_length == caret_pos))
			{
					// Append this event to the last.
				OP_STATUS status;
				mem_used -= event->BytesUsed();
				status = event->Append(str, len);
				mem_used += event->BytesUsed();

				CheckMemoryUsage();

				return status;
			}
		}
	}
	}
#endif // UNDO_EVERY_CHARACTER

	// Add event to undo
	event = UndoRedoEvent::Construct(caret_pos, str, len);
	RETURN_OOM_IF_NULL(event);

	event->Into(&undos);
	mem_used += event->BytesUsed();

	CheckMemoryUsage();

	return OpStatus::OK;
}

OP_STATUS UndoRedoStack::SubmitRemove(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str)
{
	Clear(FALSE, TRUE);

	UndoRedoEvent* event;
#ifndef UNDO_EVERY_CHARACTER
	event = PeekUndo();
	if (event)
	{
		//here we check if the user has deleted only one single char, so it
		//can be appended to the previous remove event
		if (event->GetType() == UndoRedoEvent::EV_TYPE_REMOVE &&
			(event->sel_start == event->sel_stop - 1 || event->is_appended) &&
			event->caret_pos > 0 &&
			caret_pos == event->caret_pos - 1 &&
			sel_start == sel_stop - 1)
		{
			OP_ASSERT(event->str && *event->str && event->str_length > 0);

			//this check causes events to be split on word break
			if ( !uni_isalnum(*event->str) || uni_isalnum(*removed_str) )
			{
				OP_STATUS status;
				mem_used -= event->BytesUsed();
				status = event->AppendDeleted(removed_str, sel_stop - sel_start);
				mem_used += event->BytesUsed();

				CheckMemoryUsage();

				return status;
			}
		}
	}
#endif //UNDO_EVERY_CHARACTER

	// Add event to undo
	event = UndoRedoEvent::Construct(caret_pos, sel_start, sel_stop, removed_str);
	RETURN_OOM_IF_NULL(event);

	event->Into(&undos);
	mem_used += event->BytesUsed();

	CheckMemoryUsage();

	return OpStatus::OK;
}


OP_STATUS UndoRedoStack::SubmitReplace(INT32 caret_pos, INT32 sel_start, INT32 sel_stop,
			const uni_char* removed_str, INT32 removed_str_length, 
			const uni_char* inserted_str, INT32 inserted_str_length)
{
	OP_ASSERT(removed_str && *removed_str && removed_str_length);
	OP_ASSERT(inserted_str && *inserted_str && inserted_str_length);

	UndoRedoEvent* replace_event = UndoRedoEvent::ConstructReplace(caret_pos, sel_start, sel_stop, removed_str, removed_str_length);
	RETURN_OOM_IF_NULL(replace_event);

	UndoRedoEvent* insert_event = UndoRedoEvent::Construct(caret_pos, inserted_str, inserted_str_length);
	if (insert_event == NULL)
	{
		OP_DELETE(replace_event);
		return OpStatus::ERR_NO_MEMORY;
	}

	replace_event->Into(&undos);
	insert_event->Into(&undos);
	mem_used += replace_event->BytesUsed() + insert_event->BytesUsed();

	return OpStatus::OK;
}

#ifdef WIDGETS_IME_SUPPORT
OP_STATUS UndoRedoStack::SubmitEmpty()
{
	UndoRedoEvent* empty_event = UndoRedoEvent::ConstructEmpty();
	RETURN_OOM_IF_NULL(empty_event);
	empty_event->Into(&undos);
	mem_used += empty_event->BytesUsed();
	return OpStatus::OK;
}
#endif // WIDGETS_IME_SUPPORT

UndoRedoEvent* UndoRedoStack::Undo()
{
	UndoRedoEvent* event = NULL;
	event = (UndoRedoEvent*) undos.Last();
	if (event != NULL){
		event->Out();
		event->IntoStart(&redos);
	}
	return event;
}

UndoRedoEvent* UndoRedoStack::Redo()
{
	UndoRedoEvent* event = NULL;
	event = (UndoRedoEvent*) redos.First();
	if (event != NULL){
		event->Out();
		event->Into(&undos);
	}
	return event;
}

void UndoRedoStack::CheckMemoryUsage()
{
	OP_ASSERT(redos.First() == NULL); // CheckMemoryUsage should only be called after a submit. There should be no redos here.

	while (mem_used > MAX_MEM_PER_UNDOREDOSTACK)
	{
		UndoRedoEvent* event = (UndoRedoEvent*) undos.First();
		mem_used -= event->BytesUsed();
		event->Out();
		if (event->GetType() == UndoRedoEvent::EV_TYPE_REPLACE)
		{
			//replace events exist in pairs
			UndoRedoEvent* other_event = (UndoRedoEvent*) undos.First();
			OP_ASSERT(other_event && other_event->GetType() == UndoRedoEvent::EV_TYPE_INSERT);
			mem_used -= other_event->BytesUsed();
			other_event->Out();
			OP_DELETE(other_event);
		}
		OP_DELETE(event);
	}
}

#endif // WIDGETS_UNDO_REDO_SUPPORT
