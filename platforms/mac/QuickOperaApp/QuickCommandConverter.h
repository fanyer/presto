#ifndef QuickCommandConverter_H
#define QuickCommandConverter_H

#include "platforms/mac/embrowser/EmBrowserQuick.h"

/**	A class for converting betwwen internal command numbers and standard toolbox command IDs.
*	In some circumstances, the system needs to enable some of our menu items depending on what it thinks they might do,
*	the best example is the save dialog: It will want to control the enabled state of undo, cut, copy, paste, clear
*	and select all. To do this is, the system has a very complicated set of rules for finding these items,
*	based on command key equiv, position in toolbar, title and other things.
*	To help it out, we actually use the standard system command numbers in the menu, where it makes sense (edit commands,
*	new, open, close, print, quit, preferences), and then convert it back to the
*	internal identifier, when we need to detemine enabled state or the item has been selected.
*/

class QuickCommandConverter
{
	enum { arraySize = 16 };
public:
						QuickCommandConverter();

	UInt32				AddCommandPair(EmQuickMenuCommand inQuickCommand, UInt32 inHICommandID);
	EmQuickMenuCommand	GetQuickCommandFromHICommand(UInt32 hiMenuEquv);
private:
	int					mCount;
	EmQuickMenuCommand	mQuickCommands[arraySize];
	UInt32				mHICommandIDs[arraySize];
};

extern QuickCommandConverter *gQuickCommandConverter;

#endif // HICommandConverter_H
