// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef WINDOWSCOMPLEXSCRIPT_H
#define WINDOWSCOMPLEXSCRIPT_H

namespace WIN32ComplexScriptSupport
{
	void GetTextExtent(HDC hdc, const uni_char* str, int len, SIZE* s, INT32 extra_char_spacing);
	void TextOut(HDC hdc, int x, int y, const uni_char* text, int len);
};

#endif // WINDOWSCOMPLEXSCRIPT_H
