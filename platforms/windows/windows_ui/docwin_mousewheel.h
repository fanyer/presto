//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	FILE		docwin_mousewheel.h
//	CREATED		
//	
//	DESCRIPTION	
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//  

#ifndef _DOCWIN_MOUSEWHEEL_H_INCLUDED__323219
#define _DOCWIN_MOUSEWHEEL_H_INCLUDED__323219

class OpView;
class OpWindowCommander;

void DocWin_HandleIntelliMouseWheelDown(OpWindowCommander* commander, OpView* view);
BOOL DocWin_HandleIntelliMouseMessages(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL IsPanning();
void SetPanningBeingActivated(BOOL state);
BOOL PanningBeingActivated();

#endif	//	_DOCWIN_MOUSEWHEEL_H_INCLUDED__323219 undefined
