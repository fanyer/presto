/** @file EmBrowser_mac.h
  *
  * Internal interface to Mac Specific EmBrowser functions.
  *
  * @author Jason Hazlett
  * @author Anders Markussen
  */

#ifndef EMBROWSER_MAC_H
#define EMBROWSER_MAC_H

#include "adjunct/embrowser/EmBrowser.h"

#include "adjunct/desktop_util/actions/delayed_action.h"

#include "platforms/mac/quick_support/CocoaQuickSupport.h"

#ifdef _MACINTOSH_
# define EM_BROWSER_FUNCTION_BEGIN	AutoReleaser autoreleaser;
#endif

EmBrowserStatus MacWidgetInitLibrary(EmBrowserInitParameterBlock *initPB);
EmBrowserStatus MacPostInit();
EmBrowserStatus MacWidgetShutdownLibrary();
bool IsEventInvokedFromMenu();
void TearDownCocoaSpecificElements();

#endif  //EMBROWSER_MAC_H
