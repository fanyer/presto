/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

# include "adjunct/m2/src/import/ImportFactory.h"
# include "adjunct/m2/src/import/importer.h"
# include "adjunct/m2/src/import/OperaImporter.h"
# include "adjunct/m2/src/import/EudoraImporter.h"
# include "adjunct/m2/src/import/NetscapeImporter.h"
# include "adjunct/m2/src/import/OutlookExpressImporter.h"
# include "adjunct/m2/src/import/OutlookImporter.h"
# include "adjunct/m2/src/import/AppleMailImporter.h"
# include "adjunct/m2/src/import/Opera7Importer.h"


ImportFactory::ImportFactory()
{}


OP_STATUS ImportFactory::Create(EngineTypes::ImporterId id, Importer** importer)
{
	*importer = NULL;

	switch (id)
	{
		case EngineTypes::OPERA:
			*importer = OP_NEW(OperaImporter, ());
			break;

		case EngineTypes::EUDORA:
			*importer = OP_NEW(EudoraImporter, ());
			break;
#ifdef MSWIN
		case EngineTypes::OE:
			*importer = OP_NEW(OutlookExpressImporter, ());
			break;
#endif
		case EngineTypes::OUTLOOK:
			*importer = OP_NEW(OutlookImporter, ());
			break;
#if defined _MACINTOSH_
		case EngineTypes::APPLE:
			*importer = OP_NEW(AppleMailImporter, ());
			break;
#endif
		case EngineTypes::NETSCAPE:
			*importer = OP_NEW(NetscapeImporter, ());
			break;

		case EngineTypes::MBOX:
			*importer = OP_NEW(MboxImporter, ());
			break;

		case EngineTypes::THUNDERBIRD:
			*importer = OP_NEW(NetscapeImporter, ());
			break;

		case EngineTypes::OPERA7:
			*importer = OP_NEW(Opera7Importer, ());
			break;

		default:
			break;
	}

	return (*importer) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


EngineTypes::ImporterId ImportFactory::GetDefaultImporterId()
{
#ifdef MSWIN
	return EngineTypes::OPERA7;	// to be changed!
#elif defined(_MACINTOSH_)
	return EngineTypes::APPLE;
#else
	return EngineTypes::MBOX;
#endif // MSWIN
}


ImportFactory* ImportFactory::Instance()
{
	static ImportFactory instance;
	return &instance;
}

#endif //M2_SUPPORT
