/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef OUTLOOKIMPORTER_H
#define OUTLOOKIMPORTER_H

#include "adjunct/m2/src/import/importer.h"


class OutlookImporter : public Importer
{
public:
	OutlookImporter();
	~OutlookImporter();

private:
	OutlookImporter(const OutlookImporter&);
	OutlookImporter& operator =(const OutlookImporter&);

};


#endif//OUTLOOKIMPORTER_H
