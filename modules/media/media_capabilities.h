/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_CAPABILITIES_H
#define MEDIA_CAPABILITIES_H

// MediaElement implements the changes made to the load algorithms in 2009-03.
// See http://blog.whatwg.org/2009/03. Added 2009-04-08.
#define MEDIA_CAP_HTML5_MEDIA_LOAD_ALGORITHM_200903

// MediaElement has GetPlayer method. Added 2009-04-27.
#define MEDIA_CAP_MEDIAELEMENT_HAS_GETPLAYER

// MediaElement::GetCurrentSrc returns a URL. Added 2009-04-28.
#define MEDIA_CAP_MEDIAELEMENT_GETCURRENTSRC_RETURNS_URL

// MediaElement has Suspend and Resume methods. Added 2009-05-05.
#define MEDIA_CAP_MEDIAELEMENT_HAS_SUSPEND_RESUME

// MediaElement has IsFocusable, NavigateInDirection and FocusInternal methods. Added 2009-06-09.
#define MEDIA_CAP_KEYBOARD_NAVIGATION

// MediaElement::SetSize is deprecated and MediaPlayer::SetSize is removed
#define MEDIA_CAP_NO_SETSIZE

// MediaElement has DisplayPoster. Added 2009-07-22.
#define MEDIA_CAP_DISPLAY_POSTER

#endif // !MEDIA_CAPABILITIES_H
