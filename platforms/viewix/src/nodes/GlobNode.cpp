/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/src/nodes/GlobNode.h"

GlobNode::GlobNode(const OpStringC & glob,
				   const OpStringC & mime_type,
				   GlobType type)
{
    m_glob.Set(glob);
    m_mime_type.Set(mime_type);
    m_type = type;
}

GlobNode::~GlobNode()
{
}
