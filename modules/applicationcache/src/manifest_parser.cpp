/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */


#include "core/pch.h"

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/applicationcache/src/manifest_parser.h"
#include "modules/applicationcache/src/manifest_parser_impl.h"

#include "modules/debug/debug.h"
#include "modules/util/gen_str.h"

// implementation of the pure virtual destructor
ManifestParser::~ManifestParser()  {
    // nothing to do
}

OP_STATUS ManifestParser::BuildManifestParser (
        const URL& manifest_url,
        ManifestParser*& new_manifest_parser
) {
	ManifestParserImpl *parse_impl = NULL;
    if (
    		(parse_impl = OP_NEW (ManifestParserImpl, (manifest_url))) &&
    		OpStatus::IsSuccess(parse_impl->Construct()))
    {
    	new_manifest_parser = parse_impl;
        return OpStatus::OK;
    }
    else
    {
    	OP_DELETE(parse_impl);
        new_manifest_parser = NULL;
        return OpStatus::ERR_NO_MEMORY;
    }
}

#endif // APPLICATION_CACHE_SUPPORT
