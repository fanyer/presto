#include "platforms/mac/QuickOperaApp/QuickCommandConverter.h"

QuickCommandConverter *gQuickCommandConverter = NULL;

QuickCommandConverter::QuickCommandConverter()
{
	mCount = 0;
}

UInt32 QuickCommandConverter::AddCommandPair(EmQuickMenuCommand inQuickCommand, UInt32 inHICommandID)
{
	int i;
	for (i = 0; i < mCount; i++)
	{
		if (inHICommandID == mHICommandIDs[i]) {
			mQuickCommands[i] = inQuickCommand;
			return inHICommandID;
		}
	}
	if (inHICommandID && (mCount < arraySize)) {
		mQuickCommands[mCount] = inQuickCommand;
		mHICommandIDs[mCount] = inHICommandID;
		++mCount;
		return inHICommandID;
	}
	return inQuickCommand;
}

EmQuickMenuCommand QuickCommandConverter::GetQuickCommandFromHICommand(UInt32 hiMenuEquv)
{
	int i;
	for (i = 0; i < mCount; i++)
	{
		if (hiMenuEquv == mHICommandIDs[i])
			return mQuickCommands[i];
	}
	return hiMenuEquv;
}

