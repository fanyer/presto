/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef THUMBNAILS_CAPABILITIES_H
#define THUMBNAILS_CAPABILITIES_H

// The thumbnails module is included
#define THUMBNAILS_CAP_MODULE

// ThumbnailManager::RequestThumbnail accepts the thumbnail_width and thumbnail_width arguments
#define THUMBNAILS_CAP_REQUEST_THUMBNAIL_SIZE

// It's possible to request thumbnails in ViewPortLowQuality mode
#define THUMBNAILS_CAP_LOW_QUALITY

// OpThumbnail now exposes ScaleBitmap as a public static api
#define THUMBNAILS_CAP_EXPOSES_SCALEBITMAP

#endif // THUMBNAILS_CAPABILITIES_H
