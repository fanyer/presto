/*
	File:		WidgetApp.h

*/

#ifndef WIDGET_APP_H
#define WIDGET_APP_H

void Initialize(void);
void MakeWindow(WindowRef *outRef);
void MakeMenu(void);

static pascal OSErr QuitAppleEventHandler(const AppleEvent *appleEvt, AppleEvent* reply, long refcon);
void DoAboutBox(void);

#define kAboutBox		200				// About... alert
#define kCommandAbout  'abou'   		// Command ID of About command
#define mFile 			129
#define	iQuitSeperator	10
#define iQuit 			11

// Some Basic Opera UI Commands
enum
	{
		kHICommandInit 		= FOUR_CHAR_CODE('inli'),
		kHICommandCreate 	= FOUR_CHAR_CODE('crwi'),
		kHICommandLoadURL 	= FOUR_CHAR_CODE('lurl'),
		kHICommandDestroy 	= FOUR_CHAR_CODE('desw'),
		kHICommandStop		= FOUR_CHAR_CODE('stop'),
		kHICommandReload	= FOUR_CHAR_CODE('relo'),
		kHICommandForward	= FOUR_CHAR_CODE('forw'),
		kHICommandBack		= FOUR_CHAR_CODE('back'),
		kHICommandHome		= FOUR_CHAR_CODE('if05'),
		kHICommandSearch	= FOUR_CHAR_CODE('if06'),
		kHICommandAPI1		= FOUR_CHAR_CODE('if07'),
		kHICommandAPI2		= FOUR_CHAR_CODE('if08'),
		kHICommandAPI3		= FOUR_CHAR_CODE('if09')
	};

#endif
