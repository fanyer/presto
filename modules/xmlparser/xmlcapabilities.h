/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLCAPABILITIES_H
#define XMLPARSER_XMLCAPABILITIES_H

# define XML_CAP_DOCTYPE_ENTITIES
# define XML_CAP_DOCTYPE_NOTATIONS

# define XML_CAP_IS_VALID_NAME
/* Have the functions XMLInternalParser::{IsValidNameOrNmToken,IsValidNCName,IsValidQName}. */

# define XML_CAP_DOCTYPE_REFERENCE_COUNTING
/* Have the functions XMLDoctype::IncRef and XMLDoctype::DecRef.  The
   destructor can still be used, but shouldn't be. */

# define XML_CAP_INITIALIZE_EXTRA_ARGUMENTS
/* XMLInternalParser::Initialize accepts extra arguments. */

# define XML_CAP_ISPAUSED
/* XMLInternalParser::IsPaused is supported. */

# define XML_CAP_GETERRORDESCRIPTION
/* XMLInternalParser::GetErrorDescriptiong is supported. */

# define XML_CAP_PROCESSTOKEN
/* XMLInternalParser::ProcessToken is supported. */

# define XML_CAP_TOKENHANDLERBLOCKED
/* XMLInternalParser::TokenHandlerBlocked is supported. */

# define XML_CAP_SIGNAL_INVALID_ENCODING_ERROR
/* XMLInternalParser::SignalInvalidEncodingError is supported. */

#define XML_CAP_GROW_LEAVES
/* XMLBuffer::Grow and XMLDataSource::Grow have been replaced by GrowL which leaves on OOM */

#endif // XMLPARSER_XMLCAPABILITIES_H
