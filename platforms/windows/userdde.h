
#ifdef _DDE_SUPPORT_

#ifndef _USER_DDE_H_
#define _USER_DDE_H_

#include <ddeml.h>  

#include "win_handy.h"

#define __DDEML_H
#define _HSZ_DEFINED

extern "C" HDDEDATA EXPENTRY DdeCallback(
        UINT type,      // transaction type
        UINT fmt,       // clipboard data format
        HCONV hConv,    // handle to the conversation
        HSZ hsz1,       // handle to a string
        HSZ hsz2,       // handle to a string
        HDDEDATA hData, // handle to global memory object
        DWORD dwData1,  // transaction-specific data
        DWORD dwData2);  // transaction-specific data

static uni_char *DDE_service_names[] =
{
	UNI_L("system"),
	UNI_L("WWW_OpenURL"),
	UNI_L("WWW_QueryVersion"),
 	UNI_L("WWW_Activate"),
	UNI_L("WWW_Alert"),
	UNI_L("WWW_BeginProgress"),
	UNI_L("WWW_CancelProgress"),
	UNI_L("WWW_EndProgress"),
	UNI_L("WWW_Exit"),
	UNI_L("WWW_GetWindowInfo"),
	UNI_L("WWW_ListWindows"),
	UNI_L("WWW_MakingProgress"),
	UNI_L("WWW_ParseAnchor"),
	UNI_L("WWW_QueryURLFile"),
	UNI_L("WWW_QueryViewer"),
	UNI_L("WWW_RegisterProtocol"),
	UNI_L("WWW_RegisterURLEcho"),
	UNI_L("WWW_RegisterViewer"),
	UNI_L("WWW_RegisterWindowChange"),
	UNI_L("WWW_SetProgressRange"),
	UNI_L("WWW_ShowFile"),
	UNI_L("WWW_UnRegisterProtocol"),
	UNI_L("WWW_UnRegisterURLEcho"),
	UNI_L("WWW_UnRegisterViewer"),
	UNI_L("WWW_UnRegisterWindowChange"),
	UNI_L("WWW_URLEcho"),
	UNI_L("WWW_Version"),
	UNI_L("WWW_ViewDocFile"),
	UNI_L("WWW_WindowChange")
};

class DDEHandler
{
private:

	HSZ			hszQueryVersionTopic;
	HSZ			hszActivateTopic;
	HSZ			hszAlertTopic;
	HSZ			hszBeginProgressTopic;
	HSZ			hszCancelProgressTopic;
	HSZ			hszEndProgressTopic;
	HSZ			hszExitTopic;
	HSZ			hszGetWindowInfoTopic;
	HSZ			hszListWindowsTopic;
	HSZ			hszMakingProgressTopic;		
	HSZ			hszParseAnchorTopic;
	HSZ			hszQueryURLFileTopic;
	HSZ			hszQueryViewerTopic;
	HSZ			hszRegisterProtocolTopic;
	HSZ			hszRegisterURLEchoTopic;
	HSZ			hszRegisterViewerTopic;
	HSZ			hszRegisterWindowChangeTopic;
	HSZ			hszSetProgressRangeTopic;
	HSZ			hszShowFileTopic;
	HSZ			hszUnRegisterProtocolTopic;
	HSZ			hszUnRegisterURLEchoTopic;
	HSZ			hszUnRegisterViewerTopic;
	HSZ			hszUnRegisterWindowChangeTopic;
	HSZ			hszURLEchoTopic;
	HSZ			hszVersionTopic;
	HSZ			hszViewDocFileTopic;
	HSZ			hszWindowChangeTopic;

	HSZ *phsz[29];

	HDDEDATA	hDdeData;
	
    FARPROC		lpfnDdeProc;

	BOOL		m_process;
	BOOL		m_willdie;

	BOOL 		busy;
	int			iCodePage;
	OpVector<uni_char>	m_delayed_executes;

	HDDEDATA	HandleDDESystem(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEQueryVersion() { return HandleDDEVersion(); }
	HDDEDATA	HandleDDEActivate(uni_char *str);              
	HDDEDATA	HandleDDEOpenURL(uni_char *str);                
	HDDEDATA	HandleDDECancelProgress(uni_char *str) { return NULL; }
	void		HandleDDEExit() { PostMessage(g_main_hwnd, WM_CLOSE, 0, 0L); }
	HDDEDATA	HandleDDEGetWindowInfo(uni_char *str);
	HDDEDATA	HandleDDEListWindows();
	HDDEDATA	HandleDDEParseAnchor(uni_char *str);
	HDDEDATA	HandleDDEQueryURLFile(uni_char *str);
	HDDEDATA	HandleDDEQueryViewer(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDERegisterProtocol(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDERegisterURLEcho(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDERegisterViewer(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDERegisterWindowChange(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEShowFile(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEUnRegisterProtocol(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEUnRegisterURLEcho(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEUnRegisterViewer(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEUnRegisterWindowChange(uni_char *str) { return NULL; }
	HDDEDATA	HandleDDEVersion();
	HDDEDATA	HandleDDEWindowChange(uni_char *str);
	
	void		ExitDDE(); 
	BOOL		DoDDE(const uni_char *app, const uni_char *topic, const uni_char *command, const uni_char *filename, BYTE launcher_type);

	UINT		DDEInit9x(BOOL registerIllegalServices=TRUE);
	UINT		DDEInitNT(BOOL registerIllegalServices=TRUE);

public:


	HSZ		    hszSystemTopic;        
	HSZ			hszOpenURLTopic;

				DDEHandler();
				~DDEHandler() { ExitDDE(); }

	void		InitDDE();
	BOOL		CheckConnect(HSZ hsz1);                              
	HDDEDATA	HandleRequest(HSZ hsz1, HSZ hsz2, HDDEDATA hData);
	HDDEDATA	HandlePoke(HSZ hsz1, HSZ hsz2, HDDEDATA hData);		
	HDDEDATA	ExecuteCommand(uni_char *command) { return HandleDDEOpenURL(command); }
	BOOL		RestartDDE();
	BOOL		Busy() const { return busy; }
	BOOL		SendExecCmd(uni_char* lpszCmd, uni_char* appName, uni_char* topic);
	void		Start();
	void		Stop(BOOL willdie = FALSE) {m_process=FALSE; m_willdie=willdie;}
	OP_STATUS	AddDelayedExecution(uni_char* data) {return m_delayed_executes.Add(data);}
	BOOL		IsStarted() {return m_process;}
	BOOL		WillDie()	{return m_willdie;}
};

extern DWORD DDEStrToULong(uni_char *str);

extern class DDEHandler* ddeHandler;

#endif      

#endif
