/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#import <Cocoa/Cocoa.h>

#include <Carbon/Carbon.h>
#include "platforms/mac/spellcheck/MacSpellCheckDialog.h"

@interface SpellNotify : NSResponder <NSChangeSpelling, NSIgnoreMisspelledWords>
{
}
@end

@implementation SpellNotify
- (void)changeSpelling:(id)sender;
{
	// user clicked "Fix". Grab the fixed word and paste it back into the document.
}

- (void)ignoreSpelling:(id)sender;
{
	// user clicked "Ignore". Continue.
}

@end

#ifdef __cplusplus
extern "C" {
#endif

// in SpellCheck.m
void		InitializeCocoa();
int			UniqueSpellDocumentTag();
void		CloseSpellDocumentWithTag( int tag );
CFRange		CheckSpellingOfString( CFStringRef stringToCheck, int startingOffset );
CFRange		CheckSpellingOfStringWithOptions(	CFStringRef stringToCheck, int startingOffset, CFStringRef language, Boolean wrapFlag, int tag, int wordCount );
void		IgnoreWord( CFStringRef wordToIgnore, int tag );
void		SetIgnoredWords( CFArrayRef inWords, int tag );
CFArrayRef	CopyIgnoredWordsInSpellDocumentWithTag( int tag );
CFArrayRef	GuessesForWord( CFStringRef word );

SpellNotify *notify;

void InitSpellCheck()
{
	bool sInitialised = false;
	if (!sInitialised)
	{
		InitializeCocoa();
		sInitialised = true;
		notify = [[SpellNotify alloc] init];
	}
	NSPanel* spellPanel = [[NSSpellChecker sharedSpellChecker] spellingPanel];
	BOOL visible = [spellPanel isVisible];
	if (!visible)
	{
		[spellPanel makeKeyAndOrderFront:nil];
		[spellPanel makeFirstResponder:notify];
	}

}

CFRange CheckSpellingOfStringAndDisplay(CFStringRef stringToCheck, int startingOffset, int tag)
{
	NSRange		range	= {0,0};

	NSAutoreleasePool* pool	= [[NSAutoreleasePool alloc] init];
	NSString * nsstring = (NSString*) stringToCheck;

	range = [[NSSpellChecker sharedSpellChecker] checkSpellingOfString:nsstring startingAt:startingOffset language:nil wrap:false inSpellDocumentWithTag:tag wordCount:nil];
	if (range.length > 0)
	{
		NSString * misspelled = [nsstring substringWithRange:range];
		[[NSSpellChecker sharedSpellChecker] updateSpellingPanelWithMisspelledWord:misspelled];
		[misspelled release];
	}

	[pool release];

	return( *(CFRange*)&range );	//	If there are no misspellings then it will get a length of zero
}

void SetSpellingPanelPosition(int x, int y, int width, int height)
{
	// Ignore
	// NSWindow
	// Ignore
}

#ifdef __cplusplus
}
#endif

