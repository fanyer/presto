#ifndef MACHO_COMPATIBILITY_H
#define MACHO_COMPATIBILITY_H

// These headers are in the 10.5 SDK or greater
#include <OpenGL/gl.h>
//#include "platforms/vega_backends/opengl/vegaglapi.h"
#include <OpenGL/CGLTypes.h>

void InitializeCoreGraphics();
void InitializeIOSurface();
void InitializeLaunchServices();
size_t CGDisplayBitsPerPixelLocalImpl(CGDirectDisplayID displayId);

#define CFB_CALL(a) a

// For those special circumstances, where the api was undocumented or not available before Jagwyre
#define CFB_DECLARE_PTR(a) a##ProcPtr a = NULL;
#define CFB_DECLARE_EXTPTR(a) extern a##ProcPtr a;
#define CFB_DECLARE_PRIVATE_EXTPTR(a) extern a##ProcPtr a##Ptr;
#define CFB_INIT_FUNCTION(a) a = (a##ProcPtr) CFBundleGetFunctionPointerForName(sysBundle, CFSTR(#a));
#define CFB_INIT_PRIVATE_FUNCTION(a) a##Ptr = (a##ProcPtr) CFBundleGetFunctionPointerForName(sysBundle, CFSTR(#a));
#define CFB_DECLARE_FUNCTION(result, a) typedef result (*a##ProcPtr)
#define CFB_INIT_DATAPTR(type, a) a = *((type*)CFBundleGetDataPointerForName(sysBundle,CFSTR(#a)));
#define CFB_INIT_PRIVATE_DATAPTR(type, a) a##Ptr = *((type*)CFBundleGetDataPointerForName(sysBundle, CFSTR(#a)));
#define CFB_DECLARE_DATAPTR(type, a) type a;
#define CFB_DECLARE_EXTDATAPTR(type, a) extern type a;
#define CFB_DECLARE_PRIVATE_EXTDATAPTR(type, a) extern type a##Ptr;

#ifndef MAC_OS_X_VERSION_10_7
#define MAC_OS_X_VERSION_10_7 1070
#endif

// Begin Carbon stuff

#ifdef __cplusplus
extern "C" {
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
typedef UInt32 HIThemeFocusRing;
CFB_DECLARE_FUNCTION(OSStatus, HIThemeBeginFocus)(CGContextRef inContext,HIThemeFocusRing inRing, void * inReserved);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeEndFocus)(CGContextRef inContext);
CFB_DECLARE_EXTPTR(HIThemeBeginFocus);
CFB_DECLARE_EXTPTR(HIThemeEndFocus);
#endif

// This appears to still be needed on intel?
CFB_DECLARE_FUNCTION(void, CGContextSetCompositeOperation)(CGContextRef context, int operation);
CFB_DECLARE_PRIVATE_EXTPTR(CGContextSetCompositeOperation)
#define CGContextSetCompositeOperation CGContextSetCompositeOperationPtr

#ifdef NO_CARBON_HEADERS

CFB_DECLARE_EXTDATAPTR(CFStringRef, kTISPropertyUnicodeKeyLayoutData);

// Text Services
typedef struct __TISInputSource*        TISInputSourceRef;

#ifndef SIXTY_FOUR_BIT
// Carbon events
typedef struct OpaqueEventRef*          EventRef;
typedef struct OpaqueEventHandlerRef*   EventHandlerRef;
typedef struct OpaqueEventHandlerCallRef*  EventHandlerCallRef;
typedef struct OpaqueEventTargetRef*    EventTargetRef;
typedef OSType                          EventParamName;
typedef OSType                          EventParamType;
struct ScriptLanguageRecord {
  ScriptCode          fScript;
  LangCode            fLanguage;
};
struct EventTypeSpec {
  UInt32              eventClass;
  UInt32              eventKind;
};
typedef UInt16                          EventKind;
enum {
  nullEvent                     = 0,
  mouseDown                     = 1,
  mouseUp                       = 2,
  keyDown                       = 3,
  keyUp                         = 4,
  autoKey                       = 5,
  updateEvt                     = 6,
  diskEvt                       = 7,
  activateEvt                   = 8,
//  osEvt                         = 15,
  kHighLevelEvent               = 23
};
enum {
  kEventClassTextInput          = 'text',
  kEventParamTextInputSendText  = 'tstx', /*    typeUnicodeText (if TSMDocument is Unicode), otherwise typeChar*/
  kEventParamTextInputSendSLRec = 'tssl', /*    typeIntlWritingCode*/
  kEventParamTextInputSendKeyboardEvent = 'tske', /*    typeEventRef*/
  kEventParamKeyCode            = 'kcod', /* typeUInt32*/
  kEventTextInputUnicodeForKeyEvent = 2
};
#endif // SIXTY_FOUR_BIT

typedef struct OpaqueControlRef*        ControlRef;
typedef SInt16                          ControlPartCode;

// HI commands
enum {
  kHICommandOK                  = 'ok  ',
  kHICommandCancel              = 'not!',
  kHICommandQuit                = 'quit',
  kHICommandUndo                = 'undo',
  kHICommandRedo                = 'redo',
  kHICommandCut                 = 'cut ',
  kHICommandCopy                = 'copy',
  kHICommandPaste               = 'past',
  kHICommandClear               = 'clea',
  kHICommandSelectAll           = 'sall',
  kHICommandHide                = 'hide',
  kHICommandHideOthers          = 'hido',
  kHICommandShowAll             = 'shal',
  kHICommandPreferences         = 'pref',
  kHICommandZoomWindow          = 'zoom',
  kHICommandMinimizeWindow      = 'mini',
  kHICommandMinimizeAll         = 'mina',
  kHICommandMaximizeWindow      = 'maxi',
  kHICommandMaximizeAll         = 'maxa',
  kHICommandAbout               = 'abou',
  kHICommandNew                 = 'new ',
  kHICommandOpen                = 'open',
  kHICommandClose               = 'clos',
  kHICommandSave                = 'save',
  kHICommandSaveAs              = 'svas',
  kHICommandRevert              = 'rvrt',
  kHICommandPrint               = 'prnt',
  kHICommandPageSetup           = 'page',
  kHICommandAppHelp             = 'ahlp',

  kHICommandShowCharacterPalette = 'chrp',

  kHICommandShowSpellingPanel   = 'shsp',
  kHICommandCheckSpelling       = 'cksp',
  kHICommandChangeSpelling      = 'chsp',
  kHICommandCheckSpellingAsYouType = 'aspc',
  kHICommandIgnoreSpelling      = 'igsp',
  kHICommandLearnWord           = 'lrwd'
};
typedef UInt16                          EventModifiers;
enum {
  activeFlagBit                 = 0,    /* activate? (activateEvt and mouseDown)*/
  btnStateBit                   = 7,    /* state of button?*/
  cmdKeyBit                     = 8,    /* command key down?*/
  shiftKeyBit                   = 9,    /* shift key down?*/
  alphaLockBit                  = 10,   /* alpha lock down?*/
  optionKeyBit                  = 11,   /* option key down?*/
  controlKeyBit                 = 12,   /* control key down?*/
  rightShiftKeyBit              = 13,   /* right shift key down? Not supported on Mac OS X.*/
  rightOptionKeyBit             = 14,   /* right Option key down? Not supported on Mac OS X.*/
  rightControlKeyBit            = 15,   /* right Control key down? Not supported on Mac OS X.*/
  activeFlag                    = 1 << activeFlagBit,
  btnState                      = 1 << btnStateBit,
  cmdKey                        = 1 << cmdKeyBit,
  shiftKey                      = 1 << shiftKeyBit,
  alphaLock                     = 1 << alphaLockBit,
  optionKey                     = 1 << optionKeyBit,
  controlKey                    = 1 << controlKeyBit,
  rightShiftKey                 = 1 << rightShiftKeyBit, /* Not supported on Mac OS X.*/
  rightOptionKey                = 1 << rightOptionKeyBit, /* Not supported on Mac OS X.*/
  rightControlKey               = 1 << rightControlKeyBit /* Not supported on Mac OS X.*/
};
/* MacRoman character codes*/
enum {
  kNullCharCode                 = 0,
  kHomeCharCode                 = 1,
  kEnterCharCode                = 3,
  kEndCharCode                  = 4,
  kHelpCharCode                 = 5,
  kBellCharCode                 = 7,
  kBackspaceCharCode            = 8,
  kTabCharCode                  = 9,
  kLineFeedCharCode             = 10,
  kVerticalTabCharCode          = 11,
  kPageUpCharCode               = 11,
  kFormFeedCharCode             = 12,
  kPageDownCharCode             = 12,
  kReturnCharCode               = 13,
  kFunctionKeyCharCode          = 16,
  kCommandCharCode              = 17,   /* glyph available only in system fonts*/
  kCheckCharCode                = 18,   /* glyph available only in system fonts*/
  kDiamondCharCode              = 19,   /* glyph available only in system fonts*/
  kAppleLogoCharCode            = 20,   /* glyph available only in system fonts*/
  kEscapeCharCode               = 27,
  kClearCharCode                = 27,
  kLeftArrowCharCode            = 28,
  kRightArrowCharCode           = 29,
  kUpArrowCharCode              = 30,
  kDownArrowCharCode            = 31,
  kSpaceCharCode                = 32,
  kDeleteCharCode               = 127,
  kBulletCharCode               = 165,
  kNonBreakingSpaceCharCode     = 202
};
/* For use with Get/SetMenuItemKeyGlyph*/
enum {
  kMenuNullGlyph                = 0x00, /* Null (always glyph 1)*/
  kMenuTabRightGlyph            = 0x02, /* Tab to the right key (for left-to-right script systems)*/
  kMenuTabLeftGlyph             = 0x03, /* Tab to the left key (for right-to-left script systems)*/
  kMenuEnterGlyph               = 0x04, /* Enter key*/
  kMenuShiftGlyph               = 0x05, /* Shift key*/
  kMenuControlGlyph             = 0x06, /* Control key*/
  kMenuOptionGlyph              = 0x07, /* Option key*/
  kMenuSpaceGlyph               = 0x09, /* Space (always glyph 3) key*/
  kMenuDeleteRightGlyph         = 0x0A, /* Delete to the right key (for right-to-left script systems)*/
  kMenuReturnGlyph              = 0x0B, /* Return key (for left-to-right script systems)*/
  kMenuReturnR2LGlyph           = 0x0C, /* Return key (for right-to-left script systems)*/
  kMenuNonmarkingReturnGlyph    = 0x0D, /* Nonmarking return key*/
  kMenuPencilGlyph              = 0x0F, /* Pencil key*/
  kMenuDownwardArrowDashedGlyph = 0x10, /* Downward dashed arrow key*/
  kMenuCommandGlyph             = 0x11, /* Command key*/
  kMenuCheckmarkGlyph           = 0x12, /* Checkmark key*/
  kMenuDiamondGlyph             = 0x13, /* Diamond key*/
  kMenuAppleLogoFilledGlyph     = 0x14, /* Apple logo key (filled)*/
  kMenuParagraphKoreanGlyph     = 0x15, /* Unassigned (paragraph in Korean)*/
  kMenuDeleteLeftGlyph          = 0x17, /* Delete to the left key (for left-to-right script systems)*/
  kMenuLeftArrowDashedGlyph     = 0x18, /* Leftward dashed arrow key*/
  kMenuUpArrowDashedGlyph       = 0x19, /* Upward dashed arrow key*/
  kMenuRightArrowDashedGlyph    = 0x1A, /* Rightward dashed arrow key*/
  kMenuEscapeGlyph              = 0x1B, /* Escape key*/
  kMenuClearGlyph               = 0x1C, /* Clear key*/
  kMenuLeftDoubleQuotesJapaneseGlyph = 0x1D, /* Unassigned (left double quotes in Japanese)*/
  kMenuRightDoubleQuotesJapaneseGlyph = 0x1E, /* Unassigned (right double quotes in Japanese)*/
  kMenuTrademarkJapaneseGlyph   = 0x1F, /* Unassigned (trademark in Japanese)*/
  kMenuBlankGlyph               = 0x61, /* Blank key*/
  kMenuPageUpGlyph              = 0x62, /* Page up key*/
  kMenuCapsLockGlyph            = 0x63, /* Caps lock key*/
  kMenuLeftArrowGlyph           = 0x64, /* Left arrow key*/
  kMenuRightArrowGlyph          = 0x65, /* Right arrow key*/
  kMenuNorthwestArrowGlyph      = 0x66, /* Northwest arrow key*/
  kMenuHelpGlyph                = 0x67, /* Help key*/
  kMenuUpArrowGlyph             = 0x68, /* Up arrow key*/
  kMenuSoutheastArrowGlyph      = 0x69, /* Southeast arrow key*/
  kMenuDownArrowGlyph           = 0x6A, /* Down arrow key*/
  kMenuPageDownGlyph            = 0x6B, /* Page down key*/
  kMenuAppleLogoOutlineGlyph    = 0x6C, /* Apple logo key (outline)*/
  kMenuContextualMenuGlyph      = 0x6D, /* Contextual menu key*/
  kMenuPowerGlyph               = 0x6E, /* Power key*/
  kMenuF1Glyph                  = 0x6F, /* F1 key*/
  kMenuF2Glyph                  = 0x70, /* F2 key*/
  kMenuF3Glyph                  = 0x71, /* F3 key*/
  kMenuF4Glyph                  = 0x72, /* F4 key*/
  kMenuF5Glyph                  = 0x73, /* F5 key*/
  kMenuF6Glyph                  = 0x74, /* F6 key*/
  kMenuF7Glyph                  = 0x75, /* F7 key*/
  kMenuF8Glyph                  = 0x76, /* F8 key*/
  kMenuF9Glyph                  = 0x77, /* F9 key*/
  kMenuF10Glyph                 = 0x78, /* F10 key*/
  kMenuF11Glyph                 = 0x79, /* F11 key*/
  kMenuF12Glyph                 = 0x7A, /* F12 key*/
  kMenuF13Glyph                 = 0x87, /* F13 key*/
  kMenuF14Glyph                 = 0x88, /* F14 key*/
  kMenuF15Glyph                 = 0x89, /* F15 key*/
  kMenuControlISOGlyph          = 0x8A, /* Control key (ISO standard)*/
  kMenuEjectGlyph               = 0x8C, /* Eject key (available on Mac OS X 10.2 and later)*/
  kMenuEisuGlyph                = 0x8D, /* Japanese eisu key (available in Mac OS X 10.4 and later)*/
  kMenuKanaGlyph                = 0x8E  /* Japanese kana key (available in Mac OS X 10.4 and later)*/
};



//Appearance/HITheme
typedef CGPoint                         HIPoint;
typedef CGSize                          HISize;
typedef CGRect                          HIRect;
typedef const struct __HIShape*         HIShapeRef;
typedef UInt32                          ThemeDrawState;
enum {
  kThemeStateInactive           = 0,
  kThemeStateActive             = 1,
  kThemeStatePressed            = 2,
  kThemeStateRollover           = 6,
  kThemeStateUnavailable        = 7,
  kThemeStateUnavailableInactive = 8,
  kThemeStateDisabled           = 0,
  kThemeStatePressedUp          = 2,    /* draw with up pressed     (increment/decrement buttons) */
  kThemeStatePressedDown        = 3     /* draw with down pressed (increment/decrement buttons) */
};
typedef UInt16                          ThemeButtonValue;
enum {
  kThemeButtonOff               = 0,
  kThemeButtonOn                = 1,
  kThemeButtonMixed             = 2,
  kThemeDisclosureRight         = 0,
  kThemeDisclosureDown          = 1,
  kThemeDisclosureLeft          = 2
};
typedef UInt16                          ThemeButtonAdornment;
enum {
  kThemeAdornmentNone           = 0,
  kThemeAdornmentDefault        = (1 << 0),
  kThemeAdornmentFocus          = (1 << 2),
  kThemeAdornmentRightToLeft    = (1 << 4),
  kThemeAdornmentDrawIndicatorOnly = (1 << 5),
  kThemeAdornmentHeaderButtonLeftNeighborSelected = (1 << 6),
  kThemeAdornmentHeaderButtonRightNeighborSelected = (1 << 7),
  kThemeAdornmentHeaderButtonSortUp = (1 << 8),
  kThemeAdornmentHeaderMenuButton = (1 << 9),
  kThemeAdornmentHeaderButtonNoShadow = (1 << 10),
  kThemeAdornmentHeaderButtonShadowOnly = (1 << 11),
  kThemeAdornmentHeaderButtonNoSortArrow = (1 << 12),
  kThemeAdornmentArrowLeftArrow = (1 << 6),
  kThemeAdornmentArrowDownArrow = (1 << 7),
  kThemeAdornmentArrowDoubleArrow = (1 << 8),
  kThemeAdornmentArrowUpArrow   = (1 << 9),
  kThemeAdornmentNoShadow       = kThemeAdornmentHeaderButtonNoShadow,
  kThemeAdornmentShadowOnly     = kThemeAdornmentHeaderButtonShadowOnly
};
typedef UInt32                          HIThemeOrientation;
enum {
  kHIThemeOrientationNormal     = 0,
  kHIThemeOrientationInverted   = 1
};
// Buttons
typedef UInt16                          ThemeButtonKind;
enum {
  kThemePushButton              = 0,
  kThemeCheckBox                = 1,
  kThemeRadioButton             = 2,
  kThemeBevelButton             = 3,
  kThemeArrowButton             = 4,
  kThemePopupButton             = 5,
  kThemeDisclosureTriangle      = 6,
  kThemeIncDecButton            = 7,
  kThemeBevelButtonSmall        = 8,
  kThemeBevelButtonMedium       = 3,
  kThemeBevelButtonLarge        = 9,
  kThemeListHeaderButton        = 10,
  kThemeRoundButton             = 11,
  kThemeRoundButtonLarge        = 12,
  kThemeCheckBoxSmall           = 13,
  kThemeRadioButtonSmall        = 14,
  kThemeRoundedBevelButton      = 15,
  kThemeComboBox                = 16,
  kThemeComboBoxSmall           = 17,
  kThemeComboBoxMini            = 18, // <- 10.3 and newer
  kThemeCheckBoxMini            = 19,
  kThemeRadioButtonMini         = 20,
  kThemeIncDecButtonSmall       = 21,
  kThemeIncDecButtonMini        = 22,
  kThemeArrowButtonSmall        = 23,
  kThemeArrowButtonMini         = 24,
  kThemePushButtonNormal        = 25,
  kThemePushButtonSmall         = 26,
  kThemePushButtonMini          = 27,
  kThemePopupButtonNormal       = 28,
  kThemePopupButtonSmall        = 29,
  kThemePopupButtonMini         = 30,
  kThemeBevelButtonInset        = 31, // <- 10.4 and newer
  kThemePushButtonInset         = 32,
  kThemePushButtonInsetSmall    = 33,
  kThemeRoundButtonHelp         = 34,
  kThemeNormalCheckBox          = kThemeCheckBox, // legacy
  kThemeNormalRadioButton       = kThemeRadioButton,
  kThemeLargeBevelButton        = kThemeBevelButtonLarge,
  kThemeMediumBevelButton       = kThemeBevelButtonMedium,
  kThemeMiniCheckBox            = kThemeCheckBoxMini,
  kThemeMiniRadioButton         = kThemeRadioButtonMini,
  kThemeSmallBevelButton        = kThemeBevelButtonSmall,
  kThemeSmallCheckBox           = kThemeCheckBoxSmall,
  kThemeSmallRadioButton        = kThemeRadioButtonSmall,
  kThemeLargeRoundButton        = kThemeRoundButtonLarge,
  kThemeDisclosureButton        = kThemeDisclosureTriangle
};


// Tabs
typedef UInt16                          ThemeTabStyle;
enum {
  kThemeTabNonFront             = 0,
  kThemeTabNonFrontPressed      = 1,
  kThemeTabNonFrontInactive     = 2,
  kThemeTabFront                = 3,
  kThemeTabFrontInactive        = 4,
  kThemeTabNonFrontUnavailable  = 5,
  kThemeTabFrontUnavailable     = 6
};
typedef UInt16                          ThemeTabDirection;
enum {
  kThemeTabNorth                = 0,
  kThemeTabSouth                = 1,
  kThemeTabEast                 = 2,
  kThemeTabWest                 = 3
};
typedef UInt32                          HIThemeTabSize;
enum {
  kHIThemeTabSizeNormal         = 0,
  kHIThemeTabSizeSmall          = 1,
  kHIThemeTabSizeMini           = 3
};
typedef UInt32                          HIThemeTabKind;
enum {
  kHIThemeTabKindNormal         = 0
};
typedef UInt32                          HIThemeTabAdornment;
enum {
  kHIThemeTabAdornmentNone      = 0,
  kHIThemeTabAdornmentFocus     = (1 << 2), /* to match button focus adornment */
  kHIThemeTabAdornmentLeadingSeparator = (1 << 3),
  kHIThemeTabAdornmentTrailingSeparator = (1 << 4)
};
typedef UInt32                          HIThemeTabPaneAdornment;
enum {
  kHIThemeTabPaneAdornmentNormal = 0
};
typedef UInt32                          HIThemeTabPosition;
enum {
  kHIThemeTabPositionFirst      = 0,
  kHIThemeTabPositionMiddle     = 1,
  kHIThemeTabPositionLast       = 2,
  kHIThemeTabPositionOnly       = 3
};
// Backgrounds
typedef UInt32                          ThemeBackgroundKind;
enum {
  kThemeBackgroundTabPane       = 1,
  kThemeBackgroundPlacard       = 2,
  kThemeBackgroundWindowHeader  = 3,
  kThemeBackgroundListViewWindowHeader = 4,
  kThemeBackgroundSecondaryGroupBox = 5,
  kThemeBackgroundMetal         = 6
};
// Arrows
typedef UInt16                          ThemeArrowOrientation;
enum {
  kThemeArrowLeft               = 0,
  kThemeArrowDown               = 1,
  kThemeArrowRight              = 2,
  kThemeArrowUp                 = 3
};
typedef UInt16                          ThemePopupArrowSize;
enum {
  kThemeArrow3pt                = 0,
  kThemeArrow5pt                = 1,
  kThemeArrow7pt                = 2,
  kThemeArrow9pt                = 3
};
// Tracks
typedef UInt16                          ThemeTrackKind;
enum {
  kThemeScrollBarMedium         = 0,
  kThemeScrollBarSmall          = 1,
  kThemeSliderMedium            = 2,
  kThemeProgressBarMedium       = 3,
  kThemeIndeterminateBarMedium  = 4,
  kThemeRelevanceBar            = 5,
  kThemeSliderSmall             = 6,
  kThemeProgressBarLarge        = 7,
  kThemeIndeterminateBarLarge   = 8,
  kThemeScrollBarMini           = 9, // <- 10.3 and newer
  kThemeSliderMini              = 10,
  kThemeProgressBarMini         = 11,
  kThemeIndeterminateBarMini    = 12,
  kThemeMediumScrollBar         = kThemeScrollBarMedium, // legacy
  kThemeSmallScrollBar          = kThemeScrollBarSmall,
  kThemeMediumSlider            = kThemeSliderMedium,
  kThemeMediumProgressBar       = kThemeProgressBarMedium,
  kThemeMediumIndeterminateBar  = kThemeIndeterminateBarMedium,
  kThemeSmallSlider             = kThemeSliderSmall,
  kThemeLargeProgressBar        = kThemeProgressBarLarge,
  kThemeLargeIndeterminateBar   = kThemeIndeterminateBarLarge,
  kThemeMiniScrollBar           = kThemeScrollBarMini,
  kThemeMiniSlider              = kThemeSliderMini,
  kThemeMiniProgressBar         = kThemeProgressBarMini,
  kThemeMiniIndeterminateBar    = kThemeIndeterminateBarMini
};
typedef UInt16                          ThemeTrackAttributes;
enum {
  kThemeTrackHorizontal         = (1 << 0),
  kThemeTrackRightToLeft        = (1 << 1),
  kThemeTrackShowThumb          = (1 << 2),
  kThemeTrackThumbRgnIsNotGhost = (1 << 3),
  kThemeTrackNoScrollBarArrows  = (1 << 4),
  kThemeTrackHasFocus           = (1 << 5)
};
typedef UInt8                           ThemeTrackEnableState;
enum {
  kThemeTrackActive             = 0,
  kThemeTrackDisabled           = 1,
  kThemeTrackNothingToScroll    = 2,
  kThemeTrackInactive           = 3
};
typedef UInt8                           ThemeTrackPressState;
enum {
  kThemeLeftOutsideArrowPressed = 0x01,
  kThemeLeftInsideArrowPressed  = 0x02,
  kThemeLeftTrackPressed        = 0x04,
  kThemeThumbPressed            = 0x08,
  kThemeRightTrackPressed       = 0x10,
  kThemeRightInsideArrowPressed = 0x20,
  kThemeRightOutsideArrowPressed = 0x40,
  kThemeTopOutsideArrowPressed  = kThemeLeftOutsideArrowPressed,
  kThemeTopInsideArrowPressed   = kThemeLeftInsideArrowPressed,
  kThemeTopTrackPressed         = kThemeLeftTrackPressed,
  kThemeBottomTrackPressed      = kThemeRightTrackPressed,
  kThemeBottomInsideArrowPressed = kThemeRightInsideArrowPressed,
  kThemeBottomOutsideArrowPressed = kThemeRightOutsideArrowPressed
};
typedef UInt8                           ThemeThumbDirection;
enum {
  kThemeThumbPlain              = 0,
  kThemeThumbUpward             = 1,
  kThemeThumbDownward           = 2
};
typedef UInt16                          ThemeScrollBarArrowStyle;
enum {
  kThemeScrollBarArrowsSingle   = 0,    /* single arrow on each end*/
  kThemeScrollBarArrowsLowerRight = 1   /* double arrows only on right or bottom*/
};
// Grow
typedef UInt32                          HIThemeGrowBoxKind;
enum {
  kHIThemeGrowBoxKindNormal     = 0,
  kHIThemeGrowBoxKindNone       = 1
};
typedef UInt32                          HIThemeGrowBoxSize;
enum {
  kHIThemeGrowBoxSizeNormal     = 0,
  kHIThemeGrowBoxSizeSmall      = 1
};
typedef UInt16                          ThemeGrowDirection;
enum {
  kThemeGrowLeft                = (1 << 0), /* can grow to the left */
  kThemeGrowRight               = (1 << 1), /* can grow to the right */
  kThemeGrowUp                  = (1 << 2), /* can grow up */
  kThemeGrowDown                = (1 << 3) /* can grow down */
};
// Text
enum {
  kHIThemeTextInfoVersionZero   = 0
};
typedef UInt32                          HIThemeTextTruncation;
enum {
  kHIThemeTextTruncationNone    = 0,
  kHIThemeTextTruncationMiddle  = 1,
  kHIThemeTextTruncationEnd     = 2
};
typedef UInt32                          HIThemeTextHorizontalFlush;
enum {
  kHIThemeTextHorizontalFlushLeft = 0,
  kHIThemeTextHorizontalFlushCenter = 1,
  kHIThemeTextHorizontalFlushRight = 2
};
typedef UInt32                          HIThemeTextVerticalFlush;
enum {
  kHIThemeTextVerticalFlushTop  = 0,
  kHIThemeTextVerticalFlushCenter = 1,
  kHIThemeTextVerticalFlushBottom = 2
};
typedef OptionBits                      HIThemeTextBoxOptions;
enum {
  kHIThemeTextBoxOptionNone     = 0,
  kHIThemeTextBoxOptionStronglyVertical = (1 << 1)
};
typedef UInt16                          ThemeFontID;
enum {
  kThemeSystemFont              = 0,
  kThemeSmallSystemFont         = 1,
  kThemeSmallEmphasizedSystemFont = 2,
  kThemeToolbarFont             = 108
};
typedef SInt16                          ThemeTextColor;
enum {
  kThemeTextColorDialogActive   = 1,
  kThemeTextColorDialogInactive = 2,
  kThemeTextColorAlertActive    = 3,
  kThemeTextColorAlertInactive  = 4,
  kThemeTextColorModelessDialogActive = 5,
  kThemeTextColorModelessDialogInactive = 6,
  kThemeTextColorWindowHeaderActive = 7,
  kThemeTextColorWindowHeaderInactive = 8,
  kThemeTextColorPlacardActive  = 9,
  kThemeTextColorPlacardInactive = 10,
  kThemeTextColorPlacardPressed = 11,
  kThemeTextColorPushButtonActive = 12,
  kThemeTextColorPushButtonInactive = 13,
  kThemeTextColorPushButtonPressed = 14,
  kThemeTextColorBevelButtonActive = 15,
  kThemeTextColorBevelButtonInactive = 16,
  kThemeTextColorBevelButtonPressed = 17,
  kThemeTextColorPopupButtonActive = 18,
  kThemeTextColorPopupButtonInactive = 19,
  kThemeTextColorPopupButtonPressed = 20,
  kThemeTextColorIconLabel      = 21,
  kThemeTextColorListView       = 22,
  kThemeTextColorDocumentWindowTitleActive = 23,
  kThemeTextColorDocumentWindowTitleInactive = 24,
  kThemeTextColorMovableModalWindowTitleActive = 25,
  kThemeTextColorMovableModalWindowTitleInactive = 26,
  kThemeTextColorUtilityWindowTitleActive = 27,
  kThemeTextColorUtilityWindowTitleInactive = 28,
  kThemeTextColorPopupWindowTitleActive = 29,
  kThemeTextColorPopupWindowTitleInactive = 30,
  kThemeTextColorRootMenuActive = 31,
  kThemeTextColorRootMenuSelected = 32,
  kThemeTextColorRootMenuDisabled = 33,
  kThemeTextColorMenuItemActive = 34,
  kThemeTextColorMenuItemSelected = 35,
  kThemeTextColorMenuItemDisabled = 36,
  kThemeTextColorPopupLabelActive = 37,
  kThemeTextColorPopupLabelInactive = 38,
  kThemeTextColorTabFrontActive = 39,
  kThemeTextColorTabNonFrontActive = 40,
  kThemeTextColorTabNonFrontPressed = 41,
  kThemeTextColorTabFrontInactive = 42,
  kThemeTextColorTabNonFrontInactive = 43,
  kThemeTextColorIconLabelSelected = 44,
  kThemeTextColorBevelButtonStickyActive = 45,
  kThemeTextColorBevelButtonStickyInactive = 46,
  kThemeTextColorNotification   = 47,
  kThemeTextColorSystemDetail   = 48,
  kThemeTextColorBlack          = -1,
  kThemeTextColorWhite          = -2
};
typedef UInt32                          ThemeMetric;
enum {
  kThemeMetricEditTextWhitespace = 4,
  kThemeMetricEditTextFrameOutset = 5,
  kThemeMetricListBoxFrameOutset = 6,
  kThemeMetricPushButtonHeight  = 19,
  kThemeMetricSmallCheckBoxHeight = 21,
  kThemeMetricDisclosureTriangleHeight = 25,
  kThemeMetricDisclosureTriangleWidth = 26,
  kThemeMetricPopupButtonHeight = 30,
  kThemeMetricSmallPopupButtonHeight = 31,
  kThemeMetricLargeProgressBarThickness = 32,
  kThemeMetricSmallPushButtonHeight = 35,
  kThemeMetricSmallRadioButtonHeight = 36,
  kThemeMetricSmallCheckBoxWidth = 51,
  kThemeMetricSmallRadioButtonWidth = 53,
  kThemeMetricProgressBarShadowOutset = 59,
  kThemeMetricMiniPopupButtonHeight = 96,
  kThemeMetricMiniPushButtonHeight = 98
};
typedef UInt32                          HIThemeFrameKind;
enum {
  kHIThemeFrameTextFieldSquare  = 0,
  kHIThemeFrameListBox          = 1
};
typedef UInt32                          HIThemeHeaderKind;
enum {
  kHIThemeHeaderKindWindow      = 0,
  kHIThemeHeaderKindList        = 1
};
typedef UInt32                          HIThemeGroupBoxKind;
enum {
  kHIThemeGroupBoxKindPrimary   = 0,
  kHIThemeGroupBoxKindSecondary = 1,
  kHIThemeGroupBoxKindPrimaryOpaque = 3,
  kHIThemeGroupBoxKindSecondaryOpaque = 4
};
typedef SInt16                          ThemeBrush;
enum {
  kThemeBrushBlack              = -1,
  kThemeBrushWhite              = -2,
  kThemeBrushPrimaryHighlightColor = -3,
  kThemeBrushSecondaryHighlightColor = -4,
  kThemeBrushAlternatePrimaryHighlightColor = -5
};
typedef SInt16 AppearancePartCode;
enum {
  kAppearancePartUpButton       = 20,
  kAppearancePartDownButton     = 21,
  kAppearancePartPageUpArea     = 22,
  kAppearancePartPageDownArea   = 23,
  kControlUpButtonPart          = kAppearancePartUpButton,
  kControlDownButtonPart        = kAppearancePartDownButton,
  kControlPageUpPart            = kAppearancePartPageUpArea,
  kControlPageDownPart          = kAppearancePartPageDownArea
};

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
	enum {
		pixPurgeBit                   = 0,
		noNewDeviceBit                = 1,
		useTempMemBit                 = 2,
		keepLocalBit                  = 3,
		useDistantHdwrMemBit          = 4,
		useLocalHdwrMemBit            = 5,
		pixelsPurgeableBit            = 6,
		pixelsLockedBit               = 7,
		nativeEndianPixMapBit         = 8,
		mapPixBit                     = 16,
		newDepthBit                   = 17,
		alignPixBit                   = 18,
		newRowBytesBit                = 19,
		reallocPixBit                 = 20,
		clipPixBit                    = 28,
		stretchPixBit                 = 29,
		ditherPixBit                  = 30,
		gwFlagErrBit                  = 31
	};

	enum {
		pixPurge                      = 1L << pixPurgeBit,
		noNewDevice                   = 1L << noNewDeviceBit,
		useTempMem                    = 1L << useTempMemBit,
		keepLocal                     = 1L << keepLocalBit,
		useDistantHdwrMem             = 1L << useDistantHdwrMemBit,
		useLocalHdwrMem               = 1L << useLocalHdwrMemBit,
		pixelsPurgeable               = 1L << pixelsPurgeableBit,
		pixelsLocked                  = 1L << pixelsLockedBit,
		kNativeEndianPixMap           = 1L << nativeEndianPixMapBit,
		kAllocDirectDrawSurface       = 1L << 14,
		mapPix                        = 1L << mapPixBit,
		newDepth                      = 1L << newDepthBit,
		alignPix                      = 1L << alignPixBit,
		newRowBytes                   = 1L << newRowBytesBit,
		reallocPix                    = 1L << reallocPixBit,
		clipPix                       = 1L << clipPixBit,
		stretchPix                    = 1L << stretchPixBit,
		ditherPix                     = 1L << ditherPixBit,
		gwFlagErr                     = 1L << gwFlagErrBit
	};
#endif

#ifdef SIXTY_FOUR_BIT
#pragma pack(push, 2)
#else
#pragma options align=mac68k

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
	typedef UInt16	EventKind;
#endif
	
struct EventRecord {
  EventKind           what;
  UInt32              message;
  UInt32              when;
  Point               where;
  EventModifiers      modifiers;
};
#endif // SIXTY_FOUR_BIT

struct ThemeButtonDrawInfo {
  ThemeDrawState      state;
  ThemeButtonValue    value;
  ThemeButtonAdornment  adornment;
};
struct HIThemeTabPaneDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  ThemeTabDirection   direction;
  HIThemeTabSize      size;
  HIThemeTabKind      kind;
  HIThemeTabPaneAdornment  adornment;
};
struct HIThemeTabDrawInfo {
  UInt32              version;
  ThemeTabStyle       style;
  ThemeTabDirection   direction;
  HIThemeTabSize      size;
  HIThemeTabAdornment  adornment;
  HIThemeTabKind      kind;
  HIThemeTabPosition  position;
};
struct HIThemeBackgroundDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  ThemeBackgroundKind  kind;
};
struct ScrollBarTrackInfo {
  SInt32              viewsize;
  ThemeTrackPressState  pressState;
};
struct SliderTrackInfo {
  ThemeThumbDirection  thumbDir;
  ThemeTrackPressState  pressState;
};
struct ProgressTrackInfo {
  UInt8               phase;
};
struct ThemeTrackDrawInfo {
  ThemeTrackKind      kind;
  Rect                bounds;
  SInt32              min;
  SInt32              max;
  SInt32              value;
  UInt32              reserved;
  ThemeTrackAttributes  attributes;
  ThemeTrackEnableState  enableState;
  UInt8               filler1;
  union {
    ScrollBarTrackInfo  scrollbar;
    SliderTrackInfo     slider;
    ProgressTrackInfo   progress;
  }                       trackInfo;
};
struct HIThemeGrowBoxDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  HIThemeGrowBoxKind  kind;
  ThemeGrowDirection  direction;
  HIThemeGrowBoxSize  size;
};
struct HIThemeTextInfo {
  UInt32              version;                /* current version is 0 */
  ThemeDrawState      state;
  ThemeFontID         fontID;
  HIThemeTextHorizontalFlush  horizontalFlushness; /* kHIThemeTextHorizontalFlush[Left/Center/Right] */
  HIThemeTextVerticalFlush  verticalFlushness; /* kHIThemeTextVerticalFlush[Top/Center/Bottom] */
  HIThemeTextBoxOptions  options;             /* includes kHIThemeTextBoxOptionStronglyVertical */
  HIThemeTextTruncation  truncationPosition;  /* kHIThemeTextTruncation[None/Middle/End], If none the following field is ignored */
  UInt32              truncationMaxLines;     /* the maximum number of lines before truncation occurs */
  Boolean             truncationHappened;     /* on output, whether truncation needed to happen */
};
struct HIThemeChasingArrowsDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  UInt32              index;
};
struct HIThemePopupArrowDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  ThemeArrowOrientation  orientation;
  ThemePopupArrowSize  size;
};
struct HIThemeFrameDrawInfo {
  UInt32              version;
  HIThemeFrameKind    kind;
  ThemeDrawState      state;
  Boolean             isFocused;
};
struct HIThemeHeaderDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  HIThemeHeaderKind   kind;
};
struct HIThemePlacardDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
};
struct HIThemeGroupBoxDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  HIThemeGroupBoxKind  kind;
};
struct HIThemeAnimationTimeInfo {
  CFAbsoluteTime      start;
  CFAbsoluteTime      current;
};
struct HIThemeAnimationFrameInfo {
  UInt32              index;
};
struct HIThemeButtonDrawInfo {
  UInt32              version;
  ThemeDrawState      state;
  ThemeButtonKind     kind;
  ThemeButtonValue    value;
  ThemeButtonAdornment  adornment;
  union {
    HIThemeAnimationTimeInfo  time;
    HIThemeAnimationFrameInfo  frame;
  }                       animation;
};
struct HIThemeTrackDrawInfo {
  UInt32              version;                /* current version is 0 */
  ThemeTrackKind      kind;                   /* what kind of track this info is for */
  HIRect              bounds;                 /* track basis rectangle */
  SInt32              min;                    /* min track value */
  SInt32              max;                    /* max track value */
  SInt32              value;                  /* current thumb value */
  UInt32              reserved;
  ThemeTrackAttributes  attributes;           /* various track attributes */
  ThemeTrackEnableState  enableState;         /* enable state */
  UInt8               filler1;
  union {
    ScrollBarTrackInfo  scrollbar;
    SliderTrackInfo     slider;
    ProgressTrackInfo   progress;
  }                       trackInfo;
};

enum {
	kUIModeNormal                 = 0,
	kUIModeContentSuppressed      = 1,
	kUIModeContentHidden          = 2,
	kUIModeAllSuppressed          = 4,
	kUIModeAllHidden              = 3
};
typedef UInt32                          SystemUIMode;

enum {
	kUIOptionAutoShowMenuBar      = 1 << 0,
	kUIOptionDisableAppleMenu     = 1 << 2,
	kUIOptionDisableProcessSwitch = 1 << 3,
	kUIOptionDisableForceQuit     = 1 << 4,
	kUIOptionDisableSessionTerminate = 1 << 5,
	kUIOptionDisableHide          = 1 << 6,
	kUIOptionDisableMenuBarTransparency = 1 << 9
};
typedef OptionBits                      SystemUIOptions;
	
#if !CGFLOAT_DEFINED
#ifdef SIXTY_FOUR_BIT
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#endif

struct HIScrollBarTrackInfo {
	UInt32              version;			/* The version of this data structure. Currently, it is always 0. */
	ThemeTrackEnableState  enableState;		/* The ThemeTrackEnableState for the scroll bar to be drawn. */
	ThemeTrackPressState  pressState;		/* The ThemeTrackPressState for the scroll bar to be drawn. */
	CGFloat             viewsize;			/* The view range size. */
};
typedef struct HIScrollBarTrackInfo HIScrollBarTrackInfo;

typedef struct OpaqueKeyboardLayoutRef*  KeyboardLayoutRef;
typedef UInt32 KeyboardLayoutPropertyTag;
enum {
  kKLKCHRData                   = 0,
  kKLuchrData                   = 1
};

#ifdef SIXTY_FOUR_BIT
typedef int FSIORefNum;
#else
typedef SInt16 FSIORefNum;
#endif

#ifdef SIXTY_FOUR_BIT
#pragma pack(pop)
#else
typedef UInt32 WindowClass;
enum {
  kOverlayWindowClass           = 14
};
typedef UInt32                          WindowAttributes;
enum {
  kWindowNoAttributes           = 0L
};
#pragma options align=reset
#endif

#endif // NO_CARBON_HEADERS

#ifdef NO_CARBON

#ifndef SIXTY_FOUR_BIT
typedef OSStatus (*EventHandlerProcPtr)(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
typedef EventHandlerProcPtr EventHandlerUPP;
#define InstallWindowEventHandler( target, handler, numTypes, list, userData, outHandlerRef ) \
       InstallEventHandler( GetWindowEventTarget( target ), (handler), (numTypes), (list), (userData), (outHandlerRef) )
#define InstallApplicationEventHandler( handler, numTypes, list, userData, outHandlerRef ) \
      InstallEventHandler( GetApplicationEventTarget(), (handler), (numTypes), (list), (userData), (outHandlerRef) )
#define GetEventTypeCount( t )  (sizeof( (t) ) / sizeof( EventTypeSpec ))
CFB_DECLARE_FUNCTION(OSStatus, CreateNewWindow)(WindowClass windowClass, WindowAttributes attributes, const Rect * contentBounds, WindowRef * outWindow);
CFB_DECLARE_FUNCTION(OSStatus, ReleaseWindow)(WindowRef inWindow);
CFB_DECLARE_FUNCTION(CGrafPtr, GetWindowPort)(WindowRef window);
CFB_DECLARE_FUNCTION(void, SetPortWindowPort)(WindowRef window);
CFB_DECLARE_FUNCTION(EventHandlerUPP, NewEventHandlerUPP)(EventHandlerProcPtr userRoutine);
CFB_DECLARE_FUNCTION(EventTargetRef, GetApplicationEventTarget)();
CFB_DECLARE_FUNCTION(EventTargetRef, GetWindowEventTarget)(WindowRef inWindow);
CFB_DECLARE_FUNCTION(OSStatus, InstallEventHandler)(EventTargetRef inTarget, EventHandlerUPP inHandler, UInt32 inNumTypes, const EventTypeSpec * inList, void * inUserData, EventHandlerRef * outRef);
CFB_DECLARE_FUNCTION(OSStatus, RemoveEventHandler)(EventHandlerRef inHandlerRef);
CFB_DECLARE_FUNCTION(OSStatus, GetEventParameter)(EventRef inEvent, EventParamName inName, EventParamType inDesiredType, EventParamType * outActualType, ByteCount inBufferSize, ByteCount * outActualSize, void * outData);
# ifdef NO_CARBON_HEADERS
CFB_DECLARE_EXTPTR(CreateNewWindow);
CFB_DECLARE_EXTPTR(ReleaseWindow);
CFB_DECLARE_EXTPTR(GetWindowPort);
CFB_DECLARE_EXTPTR(SetPortWindowPort);
CFB_DECLARE_EXTPTR(NewEventHandlerUPP);
CFB_DECLARE_EXTPTR(GetApplicationEventTarget);
CFB_DECLARE_EXTPTR(GetWindowEventTarget);
CFB_DECLARE_EXTPTR(InstallEventHandler);
CFB_DECLARE_EXTPTR(RemoveEventHandler);
CFB_DECLARE_EXTPTR(GetEventParameter);
# else // NO_CARBON_HEADERS
CFB_DECLARE_PRIVATE_EXTPTR(CreateNewWindow);
CFB_DECLARE_PRIVATE_EXTPTR(ReleaseWindow);
CFB_DECLARE_PRIVATE_EXTPTR(GetWindowPort);
CFB_DECLARE_PRIVATE_EXTPTR(SetPortWindowPort);
CFB_DECLARE_PRIVATE_EXTPTR(NewEventHandlerUPP);
CFB_DECLARE_PRIVATE_EXTPTR(GetApplicationEventTarget);
CFB_DECLARE_PRIVATE_EXTPTR(GetWindowEventTarget);
CFB_DECLARE_PRIVATE_EXTPTR(InstallEventHandler);
CFB_DECLARE_PRIVATE_EXTPTR(RemoveEventHandler);
CFB_DECLARE_PRIVATE_EXTPTR(GetEventParameter);
# define CreateNewWindow CreateNewWindowPtr
# define ReleaseWindow ReleaseWindowPtr
# define GetWindowPort GetWindowPortPtr
# define SetPortWindowPort SetPortWindowPortPtr
# define NewEventHandlerUPP NewEventHandlerUPPPtr
# define GetApplicationEventTarget GetApplicationEventTargetPtr
# define GetWindowEventTarget GetWindowEventTargetPtr
# define InstallEventHandler InstallEventHandlerPtr
# define RemoveEventHandler RemoveEventHandlerPtr
# define GetEventParameter GetEventParameterPtr
# endif // NO_CARBON_HEADERS
#endif // SIXTY_FOUR_BIT
CFB_DECLARE_FUNCTION(OSStatus, SetSystemUIMode)(SystemUIMode inMode, SystemUIOptions inOptions);
CFB_DECLARE_FUNCTION(OSStatus, GetSystemUIMode)(SystemUIMode *inMode, SystemUIOptions *inOptions);
CFB_DECLARE_FUNCTION(HIRect *, HIShapeGetBounds)(HIShapeRef inShape, HIRect * outRect);
CFB_DECLARE_FUNCTION(OSStatus, GetThemeMetric)(ThemeMetric inMetric, SInt32 * outMetric);
CFB_DECLARE_FUNCTION(OSStatus, GetThemeScrollBarArrowStyle)(ThemeScrollBarArrowStyle * outStyle);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeHitTestScrollBarArrows)(const HIRect *inScrollBarBounds, HIScrollBarTrackInfo *inTrackInfo, Boolean inIsHoriz, const HIPoint *inPtHit, HIRect *outTrackBounds, ControlPartCode *outPartCode);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeHitTestTrack)(const HIThemeTrackDrawInfo *inDrawInfo, const HIPoint *inMousePoint, ControlPartCode *outPartHit);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawTabPane)(const HIRect * inRect, const HIThemeTabPaneDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawBackground)(const HIRect * inBounds, const HIThemeBackgroundDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawTab)(const HIRect * inRect, const HIThemeTabDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation, HIRect * outLabelRect);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawPopupArrow)(const HIRect * inBounds, const HIThemePopupArrowDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawFrame)(const HIRect * inRect, const HIThemeFrameDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawChasingArrows)(const HIRect * inBounds, const HIThemeChasingArrowsDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawHeader)(const HIRect * inRect, const HIThemeHeaderDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawPlacard)(const HIRect * inRect, const HIThemePlacardDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawGroupBox)(const HIRect * inRect, const HIThemeGroupBoxDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawButton)(const HIRect * inBounds, const HIThemeButtonDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation, HIRect * outLabelRect);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawTrack)(const HIThemeTrackDrawInfo * inDrawInfo, const HIRect * inGhostRect, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawFocusRect)(const HIRect * inRect, Boolean inHasFocus, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawGenericWell)(const HIRect * inRect, const HIThemeButtonDrawInfo * inDrawInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeDrawTextBox)(CFStringRef inString, const HIRect * inBounds, HIThemeTextInfo * inTextInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeGetTrackThumbShape)(const HIThemeTrackDrawInfo *  inDrawInfo, HIShapeRef * outThumbShape);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeGetTrackBounds)(const HIThemeTrackDrawInfo * inDrawInfo, HIRect * outBounds);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeGetGrowBoxBounds)(const HIPoint * inOrigin, const HIThemeGrowBoxDrawInfo * inDrawInfo, HIRect * outBounds);
CFB_DECLARE_FUNCTION(OSStatus, HIThemeBrushCreateCGColor)(ThemeBrush inBrush, CGColorRef * outColor);
CFB_DECLARE_FUNCTION(TISInputSourceRef, TISCopyCurrentKeyboardLayoutInputSource)(void);
CFB_DECLARE_FUNCTION(void*, TISGetInputSourceProperty)(TISInputSourceRef inputSource, CFStringRef propertyKey);
CFB_DECLARE_FUNCTION(UInt32, KeyTranslate)(const void * transData, UInt16 keycode, UInt32 * state);
CFB_DECLARE_FUNCTION(void, KeyScript)(short code);
CFB_DECLARE_FUNCTION(TextEncoding, GetApplicationTextEncoding)(void);
CFB_DECLARE_FUNCTION(UInt8, LMGetKbdType)(void);
CFB_DECLARE_FUNCTION(OSStatus, KLGetCurrentKeyboardLayout)(KeyboardLayoutRef * oKeyboardLayout);
CFB_DECLARE_FUNCTION(OSStatus, KLGetKeyboardLayoutProperty)(KeyboardLayoutRef iKeyboardLayout, KeyboardLayoutPropertyTag iPropertyTag, const void ** oValue);
CFB_DECLARE_FUNCTION(OSStatus, EnableSecureEventInput)();
CFB_DECLARE_FUNCTION(OSStatus, DisableSecureEventInput)();
CFB_DECLARE_FUNCTION(Boolean, IsSecureEventInputEnabled)();
CFB_DECLARE_FUNCTION(TISInputSourceRef, TISCopyCurrentKeyboardInputSource)();
CFB_DECLARE_FUNCTION(TISInputSourceRef, TISCopyCurrentASCIICapableKeyboardLayoutInputSource)();
CFB_DECLARE_FUNCTION(OSStatus, TISSetInputMethodKeyboardLayoutOverride)(TISInputSourceRef keyboardLayout);
CFB_DECLARE_FUNCTION(OSStatus, TISSelectInputSource)(TISInputSourceRef keyboardLayout);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
#ifndef NP_NO_QUICKDRAW
CFB_DECLARE_FUNCTION(Rect*, GetRegionBounds)(RgnHandle, Rect*);
CFB_DECLARE_FUNCTION(void, DisposeGWorld)(GWorldPtr);
CFB_DECLARE_FUNCTION(void, SetGWorld)(CGrafPtr, GDHandle);
CFB_DECLARE_FUNCTION(RgnHandle, NewRgn)();
CFB_DECLARE_FUNCTION(void, SetPort)(GrafPtr);
CFB_DECLARE_FUNCTION(void, SetRectRgn)(RgnHandle,short,short, short,short);
CFB_DECLARE_FUNCTION(void, DisposeRgn)(RgnHandle);
CFB_DECLARE_FUNCTION(void, UnionRgn)(RgnHandle, RgnHandle, RgnHandle);
CFB_DECLARE_FUNCTION(Boolean, EmptyRgn)(RgnHandle);
CFB_DECLARE_FUNCTION(void, SectRgn)(RgnHandle, RgnHandle, RgnHandle);
CFB_DECLARE_FUNCTION(Boolean, EqualRect)(const Rect*, const Rect*);
CFB_DECLARE_FUNCTION(Boolean, EmptyRect)(const Rect*);
CFB_DECLARE_FUNCTION(void, OffsetRect)(Rect*,short,short);
CFB_DECLARE_FUNCTION(PixMapHandle, GetGWorldPixMap)(GWorldPtr);
CFB_DECLARE_FUNCTION(Boolean, LockPixels)(PixMapHandle);
CFB_DECLARE_FUNCTION(void, UnlockPixels)(PixMapHandle);
CFB_DECLARE_FUNCTION(void, CopyBits)(const BitMap*,const BitMap*,const Rect*,const Rect*,short,RgnHandle);
CFB_DECLARE_FUNCTION(QDErr, NewGWorld)(GWorldPtr*, short, const Rect*, CTabHandle, GDHandle, GWorldFlags);
CFB_DECLARE_FUNCTION(GDHandle, LMGetMainDevice)();
CFB_DECLARE_FUNCTION(Rect*, GetPortBounds)(CGrafPtr, Rect*);
CFB_DECLARE_FUNCTION(QDErr, NewGWorldFromPtr)(GWorldPtr*, UInt32, const Rect*, CTabHandle, GDHandle, GWorldFlags, Ptr, SInt32);
CFB_DECLARE_FUNCTION(Ptr, GetPixBaseAddr)(PixMapHandle);

CFB_DECLARE_EXTPTR(GetRegionBounds);
CFB_DECLARE_EXTPTR(DisposeGWorld);
CFB_DECLARE_EXTPTR(SetGWorld);
CFB_DECLARE_EXTPTR(NewRgn);
CFB_DECLARE_EXTPTR(SetPort);
CFB_DECLARE_EXTPTR(SetRectRgn);
CFB_DECLARE_EXTPTR(DisposeRgn);
CFB_DECLARE_EXTPTR(UnionRgn);
CFB_DECLARE_EXTPTR(EmptyRgn);
CFB_DECLARE_EXTPTR(SectRgn);
CFB_DECLARE_EXTPTR(EqualRect);
CFB_DECLARE_EXTPTR(EmptyRect);
CFB_DECLARE_EXTPTR(OffsetRect);
CFB_DECLARE_EXTPTR(GetGWorldPixMap);
CFB_DECLARE_EXTPTR(LockPixels);
CFB_DECLARE_EXTPTR(UnlockPixels);
CFB_DECLARE_EXTPTR(CopyBits);
CFB_DECLARE_EXTPTR(NewGWorld);
CFB_DECLARE_EXTPTR(LMGetMainDevice);
CFB_DECLARE_EXTPTR(GetPortBounds);
CFB_DECLARE_EXTPTR(NewGWorldFromPtr);
CFB_DECLARE_EXTPTR(GetPixBaseAddr);

CFB_DECLARE_PRIVATE_EXTPTR(GetRegionBounds);
CFB_DECLARE_PRIVATE_EXTPTR(DisposeGWorld);
CFB_DECLARE_PRIVATE_EXTPTR(SetGWorld);
CFB_DECLARE_PRIVATE_EXTPTR(NewRgn);
CFB_DECLARE_PRIVATE_EXTPTR(SetPort);
CFB_DECLARE_PRIVATE_EXTPTR(SetRectRgn);
CFB_DECLARE_PRIVATE_EXTPTR(DisposeRgn);
CFB_DECLARE_PRIVATE_EXTPTR(UnionRgn);
CFB_DECLARE_PRIVATE_EXTPTR(EmptyRgn);
CFB_DECLARE_PRIVATE_EXTPTR(SectRgn);
CFB_DECLARE_PRIVATE_EXTPTR(EqualRect);
CFB_DECLARE_PRIVATE_EXTPTR(EmptyRect);
CFB_DECLARE_PRIVATE_EXTPTR(OffsetRect);
CFB_DECLARE_PRIVATE_EXTPTR(GetGWorldPixMap);
CFB_DECLARE_PRIVATE_EXTPTR(LockPixels);
CFB_DECLARE_PRIVATE_EXTPTR(UnlockPixels);
CFB_DECLARE_PRIVATE_EXTPTR(CopyBits);
CFB_DECLARE_PRIVATE_EXTPTR(NewGWorld);
CFB_DECLARE_PRIVATE_EXTPTR(LMGetMainDevice);
CFB_DECLARE_PRIVATE_EXTPTR(GetPortBounds);

#define GetRegionBounds GetRegionBoundsPtr
#define DisposeGWorld DisposeGWorldPtr
#define SetGWorld SetGWorldPtr
#define NewRgn NewRgnPtr
#define SetRectRgn SetRectRgnPtr
#define DisposeRgn DisposeRgnPtr
#define UnionRgn UnionRgnPtr
#define EmptyRgn EmptyRgnPtr
#define SectRgn SectRgnPtr
#define EqualRect EqualRectPtr
#define EmptyRect EmptyRectPtr
#define OffsetRect OffsetRectPtr
#define GetGWorldPixMap GetGWorldPixMapPtr
#define LockPixels LockPixelsPtr
#define UnlockPixels UnlockPixelsPtr
#define CopyBits CopyBitsPtr
#define NewGWorld NewGWorldPtr
#define LMGetMainDevice LMGetMainDevicePtr
#define GetPortBounds GetPortBoundsPtr

#endif // !NP_NO_QUICKDRAW
CFB_DECLARE_FUNCTION(size_t, CGDisplayBitsPerPixel)(CGDirectDisplayID);
CFB_DECLARE_EXTPTR(CGDisplayBitsPerPixel);
CFB_DECLARE_PRIVATE_EXTPTR(CGDisplayBitsPerPixel);
#define CGDisplayBitsPerPixel CGDisplayBitsPerPixelPtr

#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7

# ifdef NO_CARBON_HEADERS
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
CFB_DECLARE_EXTPTR(HIShapeGetBounds);
#endif
CFB_DECLARE_EXTPTR(SetSystemUIMode);
CFB_DECLARE_EXTPTR(GetSystemUIMode);
CFB_DECLARE_EXTPTR(GetThemeMetric);
CFB_DECLARE_EXTPTR(GetThemeScrollBarArrowStyle);
CFB_DECLARE_EXTPTR(HIThemeHitTestScrollBarArrows);
CFB_DECLARE_EXTPTR(HIThemeHitTestTrack);
CFB_DECLARE_EXTPTR(HIThemeDrawTabPane);
CFB_DECLARE_EXTPTR(HIThemeDrawBackground);
CFB_DECLARE_EXTPTR(HIThemeDrawTab);
CFB_DECLARE_EXTPTR(HIThemeDrawPopupArrow);
CFB_DECLARE_EXTPTR(HIThemeDrawFrame);
CFB_DECLARE_EXTPTR(HIThemeDrawChasingArrows);
CFB_DECLARE_EXTPTR(HIThemeDrawHeader);
CFB_DECLARE_EXTPTR(HIThemeDrawPlacard);
CFB_DECLARE_EXTPTR(HIThemeDrawGroupBox);
CFB_DECLARE_EXTPTR(HIThemeDrawButton);
CFB_DECLARE_EXTPTR(HIThemeDrawTrack);
CFB_DECLARE_EXTPTR(HIThemeDrawFocusRect);
CFB_DECLARE_EXTPTR(HIThemeDrawGenericWell);
CFB_DECLARE_EXTPTR(HIThemeDrawTextBox);
CFB_DECLARE_EXTPTR(HIThemeGetTrackThumbShape);
CFB_DECLARE_EXTPTR(HIThemeGetTrackBounds);
CFB_DECLARE_EXTPTR(HIThemeGetGrowBoxBounds);
CFB_DECLARE_EXTPTR(HIThemeBrushCreateCGColor);
CFB_DECLARE_EXTPTR(TISCopyCurrentKeyboardLayoutInputSource);
CFB_DECLARE_EXTPTR(TISGetInputSourceProperty);
CFB_DECLARE_EXTPTR(KeyTranslate);
CFB_DECLARE_EXTPTR(KeyScript);
CFB_DECLARE_EXTPTR(GetApplicationTextEncoding);
CFB_DECLARE_EXTPTR(LMGetKbdType);
CFB_DECLARE_EXTPTR(KLGetCurrentKeyboardLayout);
CFB_DECLARE_EXTPTR(KLGetKeyboardLayoutProperty);
CFB_DECLARE_EXTPTR(EnableSecureEventInput);
CFB_DECLARE_EXTPTR(DisableSecureEventInput);
CFB_DECLARE_EXTPTR(IsSecureEventInputEnabled);
CFB_DECLARE_EXTPTR(TISCopyCurrentKeyboardInputSource);
CFB_DECLARE_EXTPTR(TISCopyCurrentASCIICapableKeyboardLayoutInputSource);
CFB_DECLARE_EXTPTR(TISSetInputMethodKeyboardLayoutOverride);
CFB_DECLARE_EXTPTR(TISSelectInputSource);
# else // NO_CARBON_HEADERS
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
CFB_DECLARE_PRIVATE_EXTPTR(HIShapeGetBounds);
#endif
CFB_DECLARE_PRIVATE_EXTPTR(SetSystemUIMode);
CFB_DECLARE_PRIVATE_EXTPTR(GetSystemUIMode);
CFB_DECLARE_PRIVATE_EXTPTR(GetThemeMetric);
CFB_DECLARE_PRIVATE_EXTPTR(GetThemeScrollBarArrowStyle);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeHitTestScrollBarArrows);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeHitTestTrack);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawTabPane);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawBackground);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawTab);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawPopupArrow);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawFrame);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawChasingArrows);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawHeader);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawPlacard);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawGroupBox);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawButton);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawTrack);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawFocusRect);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawGenericWell);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeDrawTextBox);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeGetTrackThumbShape);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeGetTrackBounds);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeGetGrowBoxBounds);
CFB_DECLARE_PRIVATE_EXTPTR(HIThemeBrushCreateCGColor);
CFB_DECLARE_PRIVATE_EXTPTR(TISCopyCurrentKeyboardLayoutInputSource);
CFB_DECLARE_PRIVATE_EXTPTR(TISGetInputSourceProperty);
CFB_DECLARE_PRIVATE_EXTPTR(KeyTranslate);
CFB_DECLARE_PRIVATE_EXTPTR(KeyScript);
CFB_DECLARE_PRIVATE_EXTPTR(GetApplicationTextEncoding);
CFB_DECLARE_PRIVATE_EXTPTR(LMGetKbdType);
CFB_DECLARE_PRIVATE_EXTPTR(KLGetCurrentKeyboardLayout);
CFB_DECLARE_PRIVATE_EXTPTR(KLGetKeyboardLayoutProperty);
CFB_DECLARE_PRIVATE_EXTPTR(EnableSecureEventInput);
CFB_DECLARE_PRIVATE_EXTPTR(DisableSecureEventInput);
CFB_DECLARE_PRIVATE_EXTPTR(IsSecureEventInputEnabled);
CFB_DECLARE_PRIVATE_EXTPTR(TISCopyCurrentKeyboardInputSource);
CFB_DECLARE_PRIVATE_EXTPTR(TISCopyCurrentASCIICapableKeyboardLayoutInputSource);
CFB_DECLARE_PRIVATE_EXTPTR(TISSetInputMethodKeyboardLayoutOverride);
CFB_DECLARE_PRIVATE_EXTPTR(TISSelectInputSource);
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
#define HIShapeGetBounds HIShapeGetBoundsPtr
#endif
#define SetSystemUIMode SetSystemUIModePtr
#define GetSystemUIMode GetSystemUIModePtr
#define GetThemeMetric GetThemeMetricPtr
#define GetThemeScrollBarArrowStyle GetThemeScrollBarArrowStylePtr
#define HIThemeHitTestScrollBarArrows HIThemeHitTestScrollBarArrowsPtr
#define HIThemeHitTestTrack HIThemeHitTestTrackPtr
#define HIThemeDrawTabPane HIThemeDrawTabPanePtr
#define HIThemeDrawBackground HIThemeDrawBackgroundPtr
#define HIThemeDrawTab HIThemeDrawTabPtr
#define HIThemeDrawPopupArrow HIThemeDrawPopupArrowPtr
#define HIThemeDrawFrame HIThemeDrawFramePtr
#define HIThemeDrawChasingArrows HIThemeDrawChasingArrowsPtr
#define HIThemeDrawHeader HIThemeDrawHeaderPtr
#define HIThemeDrawPlacard HIThemeDrawPlacardPtr
#define HIThemeDrawGroupBox HIThemeDrawGroupBoxPtr
#define HIThemeDrawButton HIThemeDrawButtonPtr
#define HIThemeDrawTrack HIThemeDrawTrackPtr
#define HIThemeDrawFocusRect HIThemeDrawFocusRectPtr
#define HIThemeDrawGenericWell HIThemeDrawGenericWellPtr
#define HIThemeDrawTextBox HIThemeDrawTextBoxPtr
#define HIThemeGetTrackThumbShape HIThemeGetTrackThumbShapePtr
#define HIThemeGetTrackBounds HIThemeGetTrackBoundsPtr
#define HIThemeGetGrowBoxBounds HIThemeGetGrowBoxBoundsPtr
#define HIThemeBrushCreateCGColor HIThemeBrushCreateCGColorPtr
#define TISCopyCurrentKeyboardLayoutInputSource TISCopyCurrentKeyboardLayoutInputSourcePtr
#define TISGetInputSourceProperty TISGetInputSourcePropertyPtr
#define KeyTranslate KeyTranslatePtr
#define KeyScript KeyScriptPtr
#define GetApplicationTextEncoding GetApplicationTextEncodingPtr
#define LMGetKbdType LMGetKbdTypePtr
#define KLGetCurrentKeyboardLayout KLGetCurrentKeyboardLayoutPtr
#define KLGetKeyboardLayoutProperty KLGetKeyboardLayoutPropertyPtr
#define EnableSecureEventInput EnableSecureEventInputPtr
#define DisableSecureEventInput DisableSecureEventInputPtr
#define IsSecureEventInputEnabled IsSecureEventInputEnabledPtr
#define TISCopyCurrentKeyboardInputSource TISCopyCurrentKeyboardInputSourcePtr
#define TISCopyCurrentASCIICapableKeyboardLayoutInputSource TISCopyCurrentASCIICapableKeyboardLayoutInputSourcePtr
#define TISSetInputMethodKeyboardLayoutOverride TISSetInputMethodKeyboardLayoutOverridePtr
#define TISSelectInputSource TISSelectInputSourcePtr
# endif // NO_CARBON_HEADERS
#endif // NO_CARBON

// IOSurface
typedef struct __IOSurface *IOSurfaceRef;
typedef uint32_t IOSurfaceID;
	
CFB_DECLARE_PRIVATE_EXTDATAPTR(CFStringRef, kIOSurfaceIsGlobal);
#define kIOSurfaceIsGlobal kIOSurfaceIsGlobalPtr
CFB_DECLARE_PRIVATE_EXTDATAPTR(CFStringRef, kIOSurfaceWidth);
#define kIOSurfaceWidth kIOSurfaceWidthPtr
CFB_DECLARE_PRIVATE_EXTDATAPTR(CFStringRef, kIOSurfaceHeight);
#define kIOSurfaceHeight kIOSurfaceHeightPtr
CFB_DECLARE_PRIVATE_EXTDATAPTR(CFStringRef, kIOSurfaceBytesPerElement);
#define kIOSurfaceBytesPerElement kIOSurfaceBytesPerElementPtr
	
	
CFB_DECLARE_FUNCTION(IOSurfaceRef, IOSurfaceCreate)(CFDictionaryRef properties);
CFB_DECLARE_PRIVATE_EXTPTR(IOSurfaceCreate);
#define IOSurfaceCreate IOSurfaceCreatePtr
CFB_DECLARE_FUNCTION(IOSurfaceRef, IOSurfaceLookup)(IOSurfaceID csid);
CFB_DECLARE_PRIVATE_EXTPTR(IOSurfaceLookup);
#define IOSurfaceLookup IOSurfaceLookupPtr
CFB_DECLARE_FUNCTION(size_t, IOSurfaceGetWidth)(IOSurfaceRef buffer);
CFB_DECLARE_PRIVATE_EXTPTR(IOSurfaceGetWidth);
#define IOSurfaceGetWidth IOSurfaceGetWidthPtr
CFB_DECLARE_FUNCTION(size_t, IOSurfaceGetHeight)(IOSurfaceRef buffer);
CFB_DECLARE_PRIVATE_EXTPTR(IOSurfaceGetHeight);
#define IOSurfaceGetHeight IOSurfaceGetHeightPtr
CFB_DECLARE_FUNCTION(IOSurfaceID, IOSurfaceGetID)(IOSurfaceRef buffer);
CFB_DECLARE_PRIVATE_EXTPTR(IOSurfaceGetID);
#define IOSurfaceGetID IOSurfaceGetIDPtr

	
CFB_DECLARE_FUNCTION(CGLError, CGLTexImageIOSurface2D)(CGLContextObj ctx, GLenum target, GLenum internal_format,
													   GLsizei width, GLsizei height, GLenum format, GLenum type, IOSurfaceRef ioSurface, GLuint plane);
CFB_DECLARE_PRIVATE_EXTPTR(CGLTexImageIOSurface2D);
#define CGLTexImageIOSurface2D CGLTexImageIOSurface2DPtr

// LaunchServices for renaming of the PluginWrapper32/64 process
	
CFB_DECLARE_EXTDATAPTR(CFStringRef, _kLSDisplayNameKey);
#define kLSDisplayNameKey _kLSDisplayNameKey
	
CFB_DECLARE_FUNCTION(CFTypeRef, _LSGetCurrentApplicationASN)();
CFB_DECLARE_EXTPTR(_LSGetCurrentApplicationASN);
#define LSGetCurrentApplicationASN _LSGetCurrentApplicationASN
CFB_DECLARE_FUNCTION(OSStatus, _LSSetApplicationInformationItem)(int constant, CFTypeRef asn, CFStringRef display_name_key, CFStringRef process_name, CFDictionaryRef* dictionary);
CFB_DECLARE_EXTPTR(_LSSetApplicationInformationItem);
#define LSSetApplicationInformationItem _LSSetApplicationInformationItem

	
// Hacky defines so Carbon and QuickDraw can exist in 64-bit
#ifdef SIXTY_FOUR_BIT
#define NPDrawingModelQuickDraw (NPDrawingModel)0
#define NPEventModelCarbon		(NPEventModel)0
#endif // SIXTY_FOUR_BIT
	
#ifdef __cplusplus
}
#endif

#endif // MACHO_COMPATIBILITY_H
