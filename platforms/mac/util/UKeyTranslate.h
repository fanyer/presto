#ifndef _U_KEY_TRANSLATE_H_
#define _U_KEY_TRANSLATE_H_

#define kMaskOutCommand	0xFE00    		// we need the modifiers without the command key
#define kMaskOutOption	0xF700			// we need the modifiers without the option key
#define kMaskVirtualKey 0x0000FF00  	// get virtual key from event message for
                                        // KeyTrans
#define kMaskASCII1     0x00FF0000    	// get the key out of the ASCII1 byte
#define kMaskASCII2     0x000000FF    	// get the key out of the ASCII2 byte


#if defined(VER_BETA)
//#define DEBUG_UNI_KEYSTROKES
#endif
#ifdef DEBUG_UNI_KEYSTROKES
# warning "THIS BUILD CONTAINS KEYBOARD DEBUG CODE!"
#endif

class UKeyTranslate
{

public:
	static OP_STATUS GetUnicharFromVirtualKey(UInt32 virtualKeyCode, uni_char &outUniChar, UInt8 modifierKeyState = 0);
};

#endif // _U_KEY_TRANSLATE_H_
