/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#if !defined(ABOUTOPERA_H) && !defined(USE_ABOUT_FRAMEWORK)
#define ABOUTOPERA_H

#include "modules/util/opstring.h"
#include "modules/hardcore/base/opstatus.h"

class URL;

class AboutOpera
{
public:
	virtual ~AboutOpera() {}
	// Live with default constructor (no-op); we have no member data.
	void generateData( URL& url ); // entire class exists for this method

protected:
	/** Platform-dependent methods.
	 *
	 * Platform-specific classes should implement these.  Anyone modifying the
	 * write*(url) methods below to make some part of the page
	 * platform-specific, or to add new platform-specific parts to the page,
	 * should mediate the platform dependence via additional get...() methods
	 * like these, deployed in similar ways (see aboutopera.cpp).  When one of
	 * these methods fails, no output should be produced for its segment.
	 *
	 * Since different platforms have different file separators, the cleanPath()
	 * method is also included here; see aboutunix.cpp for an implementation
	 * that might be worth providing as default
	 */

	//* Full version number with any beta/preview suffix in full form
	virtual OP_STATUS GetVersion(OpString *dest)
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* Build number
	virtual OP_STATUS GetBuild(OpString *dest)
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* URL of splash image (if any); will be duly packaged in <img src="...">
	virtual OP_STATUS getSplashImageURL( OpString& found )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* Platform name and version strings
	virtual OP_STATUS getPlatformDetails( OpString& name, OpString& version )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* Details of underlying application toolkit, if any (e.g. Qt, PowerParts)
	virtual OP_STATUS getToolkitDetails( OpString& name, OpString& version )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* "Java" (or substitute) and description of runtime being used
	virtual OP_STATUS getJavaVersion( OpString& title, OpString& text )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* Evaluation counter, if activated
	virtual OP_STATUS getEvaluationData( OpString& label, OpString& text )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* Name of directory of help files; include language extension if relevant
	virtual OP_STATUS getHelpDir( OpString& found )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	//* Translation for "Sound" and text about sound server (and where to get it)
	virtual OP_STATUS getSoundDoc( OpString& title, OpString& text )
	{ return OpStatus::ERR_NOT_SUPPORTED; }

	/** Tidy up a file name, suitable for showing to users.
	 *
	 * For example, eliminate repeated file separators, ensure it ends in a
	 * dangling file separator if asdir is true.  All optional; pure display.
	 */
	virtual void cleanPath( OpString& file, BOOL asdir=FALSE ) {}

private:
	// generateData broken into logical fragments for tidiness' sake
	void writePreamble( URL& url );
	void writeVersion( URL& url );
	void writePaths( URL& url );
	void writeSound( URL& url ); // no-op unless getSoundDoc is supported

	// Not real methods; could be local statics, in principal:
	void writeUA( URL& url ); // no-op unless SHOW_USER_AGENT defined
	void writeCredits( URL& url );
};

/* url_4 now includes this file, but wants the implementation class (as distinct
 * from the interface class) called AboutOpera; until such time as pi/ provides
 * AboutOpera and OpSomethingInfo provides an AboutOpera *GetAboutOpera(), we're
 * lumbered with the following bodge ...
 */
#include "aboutunix.h"
#define AboutOpera AboutUnix
#endif // !ABOUTOPERA_H, !USE_ABOUT_FRAMEWORK
