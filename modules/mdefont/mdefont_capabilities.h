/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MDEFONT_CAPABILITIES_H
#define MDEFONT_CAPABILITIES_H

#ifdef MDEFONT_MODULE

// capabilities go here

#define MDF_CAP_UPDATEGLYPHMASK ///< function MDF_UpdateGlyphMask exists

#define MDF_CAP_LINEAR_TEXT_SCALE ///< MDF_DrawString(..., MDE_BUFFER, ...) takes optional word_width

#endif // MDEFONT_MODULE

// This is always defined as it is always present (even if mdefont is not used)
#define MDF_CAP_PROCESSEDSTRING ///< mdefont module provides api and functionality for string processing

#define MDF_CAP_PROCESS_STRING_NO_ADVANCE ///< MDF_ProcessString can be requested not to accumulate advance

#endif // !MDEFONT_CAPABILITIES_H
