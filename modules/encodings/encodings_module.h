/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined ENCODINGS_MODULE_H
#define ENCODINGS_MODULE_H

#include "modules/hardcore/opera/module.h"

/** EncodingsModule must be created on startup */
# define ENCODINGS_MODULE_REQUIRED

class EncodingsModule : public OperaModule
{
public:
	EncodingsModule();

	// Inherited interfaces
	virtual void InitL(const OperaInitInfo &);
	virtual void Destroy();
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	virtual BOOL FreeCachedData(BOOL);
#endif

	// Access methods
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	inline class OpTableManager *GetOpTableManager()
	{ return m_table_manager; }
#endif

	inline class CharsetManager *GetCharsetManager()
	{ return m_charset_manager; }

private:
	class CharsetManager *m_charset_manager;

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	class OpTableManager *m_table_manager;

# ifdef ENC_BIG_HKSCS_TABLE
	/** We do not store the forward HKSCS conversion table in encoding.bin
	 * because of the size. It is much more efficient only to store the
	 * reverse tables and to compute the forward table when needed (the
	 * extended characters are not that common). Since this is large, we
	 * only do this if TWEAK_ENC_GENERATE_BIG_HKSCS_TABLE is enabled.
	 */
	UINT32 *m_hkscs_table;
	friend class Big5HKSCStoUTF16Converter;
# endif
#endif

#ifdef SELFTEST
public:
	char *buffer, *bufferback;
#endif
};

#endif // !ENCODINGS_MODULE_H
