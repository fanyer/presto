/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Anders Oredsson <anderso@opera.com> }
*/

#ifndef _PROBETOOLS_PROBENAMEMAPPER_H
#define _PROBETOOLS_PROBENAMEMAPPER_H

#ifdef SUPPORT_PROBETOOLS

class  OpProbeIdentifier;

/*
OpProbeNameMapper:

Simple static class holding the GetMappedProbeParameterName functionality.
*/
class OpProbeNameMapper{
public:

	//Put probe index_parameter name-mappings into this method.
	//Only usable in special cases, returns false if no name was copied
	static bool GetMappedProbeParameterName(OpProbeIdentifier& probe_id, char* targetBuffer, size_t targetBufferLen);
};

#endif //SUPPORT_PROBETOOLS
#endif //_PROBETOOLS_PROBENAMEMAPPER_H
