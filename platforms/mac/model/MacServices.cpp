/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/MacServices.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/CTextConverter.h"

#include "adjunct/quick/Application.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/desktop_util/mail/mailcompose.h"

#ifndef NO_CARBON
#ifdef SUPPORT_OSX_SERVICES

EventHandlerRef MacOpServices::sServicesHandler = NULL;
EventHandlerUPP MacOpServices::sServicesUPP = NULL;

MacOpServices::MacOpServices()
{
}

void MacOpServices::Free()
{
	if(sServicesHandler)
	{
		::RemoveEventHandler(sServicesHandler);
		sServicesHandler = NULL;
	}
	if(sServicesUPP)
	{
		::DisposeEventHandlerUPP(sServicesUPP);
		sServicesUPP = NULL;
	}
}

void MacOpServices::InstallServicesHandler()
{
	// Only install if we're not embedded
	if(g_application && g_application->IsEmBrowser())
		return;

	if(sServicesHandler)
		return;	// already installed

	static EventTypeSpec	servicesEventList[] =
	{
		{kEventClassService, kEventServicePerform}
	};

	sServicesUPP = NewEventHandlerUPP(ServicesHandler);

	if(!sServicesUPP)
		return;

	InstallApplicationEventHandler(	sServicesUPP,
													GetEventTypeCount(servicesEventList),
													servicesEventList,
													/*userdata*/ 0,
													&sServicesHandler);
}

pascal OSStatus	MacOpServices::ServicesHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData)
{
	UInt32 		carbonEventClass = GetEventClass(inEvent);
	UInt32 		carbonEventKind = GetEventKind(inEvent);
	bool		eventHandled 	= false;

	if((carbonEventClass == kEventClassService) && (carbonEventKind == kEventServicePerform))
	{
		CFStringRef     message;
//		CFStringRef     userData;
		ScrapRef        specificScrap;
//		ScrapRef        currentScrap;
		OSStatus		err;
/*

*/
		err = GetEventParameter (inEvent,
                kEventParamServiceMessageName,
                typeCFStringRef,
                NULL,
                sizeof (CFStringRef),
                NULL,
                &message);

        if(err != noErr)
        	goto exithandler;
        if(!message)
        	goto exithandler;

        err = GetEventParameter (inEvent,
		        kEventParamScrapRef,
                typeScrapRef,
                NULL,
                sizeof (ScrapRef),
                NULL,
                &specificScrap);

        if(err != noErr)
        	goto exithandler;

		OperaServiceProviderKind kind = GetServiceKind(message);
		if(kind == kOperaServiceKindUnknown)
			goto exithandler;

        if((kind == kOperaServiceKindOpenURL) ||
        	(kind == kOperaServiceKindMailTo))
        {
        	OpString text;
        	long textSize = 0;
        	Boolean gotData = false;
			char *macbuffer;

			err = GetScrapFlavorSize(specificScrap, kScrapFlavorTypeUnicode, &textSize);
			if ((err == noErr) && (textSize > 0))
			{
				if (text.Reserve((textSize / 2) + 1))
				{
					err = GetScrapFlavorData(specificScrap, kScrapFlavorTypeUnicode, &textSize, text.CStr());
					if (err == noErr)
					{
						text[(int)(textSize / 2)] = '\0';
						gotData = true;
					}
				}
			}
			if (!gotData)
			{
				err = GetScrapFlavorSize(specificScrap, kScrapFlavorTypeText, &textSize);
				if ((err == noErr) && (textSize > 0))
				{
					macbuffer = new char[textSize + 1];
					if (macbuffer)
					{
						if (text.Reserve(textSize + 1))
						{
							err = GetScrapFlavorData(specificScrap, kScrapFlavorTypeText, &textSize, macbuffer);
							if (err == noErr)
							{
								macbuffer[textSize] = '\0';
								textSize = gTextConverter.ConvertStringFromMacC(macbuffer, text.CStr(), textSize + 1);
								gotData = true;
							}
						}
						delete [] macbuffer;
					}
				}
			}

        	if(gotData)
        	{
        		if(text.HasContent())
        		{
        			eventHandled = true;

        			if(kind == kOperaServiceKindOpenURL)
        			{
        				g_application->OpenURL(text, MAYBE, MAYBE, MAYBE);
        			}
        			else if(kind == kOperaServiceKindMailTo)
        			{
        				OpString empty;
        				MailTo mailto;
        				mailto.Init(text, empty, empty, empty, empty);
        				MailCompose::ComposeMail(mailto);
        			}
        		}
        	}
        }
        else if((kind == kOperaServiceKindSendFiles) ||
        		(kind == kOperaServiceKindOpenFiles))
        {
        	/**
        	* Due to limitations in the scrap manager we can only get single files.
        	* See http://lists.apple.com/archives/carbon-development/2001/Sep/21/carbonapplicationandthes.007.txt.
        	*
        	* 'furl' datatype described here: http://developer.apple.com/technotes/tn/tn2022.html
        	*/

        	long textSize = 0;
        	OpString text;
        	Boolean gotData = false;

        	err = GetScrapFlavorSize(specificScrap, 'furl', &textSize);
			if ((err == noErr) && (textSize > 0))
			{
				char* utf8str = new char[textSize + 1];
				if(utf8str)
				{
					err = GetScrapFlavorData(specificScrap, 'furl', &textSize, utf8str);
					if (err == noErr)
					{
						utf8str[textSize] = '\0';

						if(kind == kOperaServiceKindSendFiles)
						{
							// We want a filesystem path, not a file:// URL
							CFURLRef cfurl = CFURLCreateWithBytes(NULL, (UInt8*)utf8str, textSize, kCFStringEncodingUTF8, NULL);
							if(cfurl)
							{
								CFStringRef path = CFURLCopyFileSystemPath(cfurl, kCFURLPOSIXPathStyle);
								if(path)
								{
									size_t strLen = CFStringGetLength(path);

									if(text.Reserve(strLen+1) != NULL)
									{
										uni_char *dataPtr = text.DataPtr();
										CFStringGetCharacters(path, CFRangeMake(0, strLen), (UniChar*)dataPtr);
										dataPtr[strLen] = 0;

										gotData = true;
									}

									CFRelease(path);
								}
								CFRelease(cfurl);
							}
						}
						else
						{
							if(OpStatus::IsSuccess(text.SetFromUTF8(utf8str, KAll)))
							{
								gotData = true;
							}
						}
					}

					delete [] utf8str;
				}
			}

			if(gotData)
			{
				if(text.HasContent())
        		{
        			eventHandled = true;

        			if(kind == kOperaServiceKindSendFiles)
        			{
        				OpString empty;
#if defined M2_SUPPORT
        				MailTo mailto;
        				mailto.Init(empty, empty, empty, empty, empty);
        				MailCompose::ComposeMail(mailto, FALSE, FALSE, FALSE, &text);
#endif
          			}
        			else if(kind == kOperaServiceKindOpenFiles)
        			{
        				g_application->OpenURL(text, MAYBE, MAYBE, MAYBE);
        			}
        		}
			}
        }
	}

exithandler:
	if (eventHandled)
		return noErr;
	else
		return eventNotHandledErr;
}

MacOpServices::OperaServiceProviderKind MacOpServices::GetServiceKind(CFStringRef cfstr)
{
	OperaServiceProviderKind kind = kOperaServiceKindUnknown;

	if(CFStringCompare(cfstr, kOperaOpenURLString, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	{
		kind = kOperaServiceKindOpenURL;
	}
	else if(CFStringCompare(cfstr, kOperaMailToString, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	{
		kind = kOperaServiceKindMailTo;
	}
	else if(CFStringCompare(cfstr, kOperaSendFilesString, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	{
		kind = kOperaServiceKindSendFiles;
	}
	else if(CFStringCompare(cfstr, kOperaOpenFilesString, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	{
		kind = kOperaServiceKindOpenFiles;
	}

	return kind;
}

#endif // SUPPORT_OSX_SERVICES
#endif // !NO_CARBON
