#include "core/pch.h"

#include "modules/pi/OpFont.h"
#include "modules/pi/OpInputMethod.h"
#include "modules/unicode/unicode_stringiterator.h"

#ifdef WIDGETS_IME_SUPPORT

OpInputMethodString::OpInputMethodString()
	: caret_pos(0)
	, candidate_pos(0)
	, candidate_len(0)
	, string(NULL)
	, show_caret(TRUE)
	, show_underline(TRUE)
	, underline_color(OP_RGB(255, 0, 0))
{
}

OpInputMethodString::~OpInputMethodString()
{
	OP_DELETEA(string);
}

void OpInputMethodString::SetCaretPos(INT32 pos)
{
	caret_pos = pos;
}

void OpInputMethodString::SetCandidate(INT32 pos, INT32 len)
{
	candidate_pos = pos;
	candidate_len = len;
}

OP_STATUS OpInputMethodString::Set(const uni_char* str, INT32 len)
{
	OP_DELETEA(string);
	string = OP_NEWA(uni_char, len + 1);
	if (string == NULL)
		return OpStatus::ERR_NO_MEMORY;
	op_memcpy(string, str, len * sizeof(uni_char));
	string[len] = '\0';
	return OpStatus::OK;
}

INT32 OpInputMethodString::GetUniPointCaretPos() const
{
	if(string == NULL)
		return 0;

	int unipoint_caret_pos = 0;
	for (UnicodeStringIterator it(string, 0, caret_pos); !it.IsAtEnd(); it.Next())
		unipoint_caret_pos++;

	return unipoint_caret_pos;
}

INT32 OpInputMethodString::GetCaretPos() const
{
	return caret_pos;
}

INT32 OpInputMethodString::GetCandidatePos() const
{
	return candidate_pos;
}

INT32 OpInputMethodString::GetCandidateLength() const
{
	return candidate_len;
}

const uni_char* OpInputMethodString::Get() const
{
	if (string == NULL)
		return UNI_L("");
	return string;
}

#endif // WIDGETS_IME_SUPPORT
