/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#ifndef __SECURE_MIME_FUNCTIONS_H__
#define __SECURE_MIME_FUNCTIONS_H__

#ifdef _MIME_SUPPORT_

// This file defines the functions used to create 
// the various MIME_Decoders for secure types, like PGP and S/MIME

class MIME_Multipart_Encrypted_Parameters;
class MIME_Security_Body;

MIME_Security_Body *CreatePGPDecoderL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type);
MIME_Multipart_Encrypted_Parameters *CreatePGPEncParametersL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type);

MIME_Decoder *CreateSecureMultipartDecoderL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type);

#endif // _MIME_SUPPORT_

#endif
