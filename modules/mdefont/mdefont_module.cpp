/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef MDEFONT_MODULE

#include "modules/mdefont/mdefont_module.h"
#include "modules/mdefont/mdefont.h"

#ifdef MDF_OPENTYPE_SUPPORT
# include "modules/mdefont/mdf_ot_indic.h"
# include "modules/util/simset.h"
#endif // MDF_OPENTYPE_SUPPORT

MDF_ProcessedGlyphBuffer::~MDF_ProcessedGlyphBuffer()
{
	OP_DELETEA(m_buffer);
}

MdefontModule::MdefontModule()
	: mdf_engine(0)
#ifdef USE_ONLY_LIMITED_FONT_SIZE_SET
	, FixedSizeTable(0)
	, NumFixedSizes(0)
#endif // USE_ONLY_LIMITED_FONT_SIZE_SET
#ifdef MDF_OPENTYPE_SUPPORT
	, m_indic_scripts(0)
	, m_ot_buflock(FALSE)
	, m_ot_ubuf(0)
	, m_ot_ibuf(0)
#endif // MDF_OPENTYPE_SUPPORT
{
}

void MdefontModule::InitL(const OperaInitInfo& info)
{
	if (mdf_engine)
		LEAVE_IF_ERROR(mdf_engine->Init());

#ifdef USE_ONLY_LIMITED_FONT_SIZE_SET
    int temp_table[] = LIMITED_FONT_SIZE_SET;
    NumFixedSizes = sizeof(temp_table) / sizeof(*temp_table);
    if (NumFixedSizes > 0)
    {
        FixedSizeTable = OP_NEWA(int, NumFixedSizes);
        if (FixedSizeTable)
            for (int i = 0; i < NumFixedSizes; ++i)
                FixedSizeTable[i] = temp_table[i];
        else
            // no proper error handling here, but there's scarcely need for it since this tweak is for testing only
            OP_ASSERT(!"could not allocate memory for fixed font-size table");
    }
#endif // USE_ONLY_LIMITED_FONT_SIZE_SET

#ifdef MDF_OPENTYPE_SUPPORT
	m_ot_ubuf = OP_NEW_L(TempBuffer, ());
	m_ot_ibuf = OP_NEW_L(TempBuffer, ());
	m_indic_scripts = OP_NEW_L(IndicGlyphClass, ());
	LEAVE_IF_ERROR( m_indic_scripts->Initialize() );
#endif // MDF_OPENTYPE_SUPPORT
}

void MdefontModule::Destroy()
{
#ifdef USE_ONLY_LIMITED_FONT_SIZE_SET
	OP_DELETEA(FixedSizeTable);
	FixedSizeTable = 0;
	NumFixedSizes = 0;
#endif // USE_ONLY_LIMITED_FONT_SIZE_SET

#ifdef MDF_OPENTYPE_SUPPORT
	OP_ASSERT(!m_ot_buflock);
	OP_DELETE(m_ot_ibuf);
	OP_DELETE(m_ot_ubuf);
	OP_DELETE(m_indic_scripts);
#endif // MDF_OPENTYPE_SUPPORT
}

#endif // MDEFONT_MODULE
