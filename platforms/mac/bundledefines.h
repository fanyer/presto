/*
 *  bundledefines.h
 *  Opera
 *
 *  Created by Adam Minchinton on 05/04/2011.
 *  Copyright 2011 Opera. All rights reserved.
 *
 */

#ifndef BUNDLE_DEFINES_H
#define BUNDLE_DEFINES_H


#define OPERA_BUNDLE_ID				com.operasoftware.Opera
#define OPERA_NEXT_BUNDLE_ID		com.operasoftware.OperaNext
#define OPERA_UPDATE_BUNDLE_ID		com.operasoftware.OperaUpdate
#define OPERA_LABS_BUNDLE_ID_PREFIX	com.operasoftware.OperaLabs						// Note: This is just the Prefix to the full bundle id


// More calculated defines

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(x) #x

#define OPERA_BUNDLE_ID_STRING				XSTRINGIFY(OPERA_BUNDLE_ID)
#define OPERA_NEXT_BUNDLE_ID_STRING			XSTRINGIFY(OPERA_NEXT_BUNDLE_ID)
#define OPERA_UPDATE_BUNDLE_ID_STRING		XSTRINGIFY(OPERA_UPDATE_BUNDLE_ID)
#define OPERA_LABS_BUNDLE_ID_PREFIX_STRING	XSTRINGIFY(OPERA_LABS_BUNDLE_ID_PREFIX)
#define OPERA_NEXT_WI_BUNDLE_ID_STRING		XSTRINGIFY(OPERA_NEXT_WI_BUNDLE_ID)

#endif // !BUNDLE_DEFINES_H

