//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	FILENAME		msghook.h
//	CREATED			DG-271198 
//
//	DESCRIPTION		MS Win message hooks
//					See CPP file for details.
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯



#ifndef _MSGHOOK_H_INCLUDED
#define	_MSGHOOK_H_INCLUDED

#define _MSGHOOK_MOUSE				//	hook mouse
#define _MSGHOOK_CALLWNDPROC		//	pre sendmessage hook
#define _MSGHOOK_GETMESSAGE			//	getmessage hook
#ifdef _DEBUG
# define _MSGHOOK_CBT			//	CBT Hook
#endif //_DEBUG

#ifndef NS4P_COMPONENT_PLUGINS
class DelayedFlashMessageHandler : private MessageObject
{
	public:
		DelayedFlashMessageHandler();
		~DelayedFlashMessageHandler();

		BOOL PostFlashMessage(MSG *msg);
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	private:
		OpVector<MSG>   m_messages;
		UINT32          m_last_time;
};
#endif // !NS4P_COMPONENT_PLUGINS

	BOOL	InstallMSWinMsgHooks	();
	void	RemoveMSWinMsgHooks		();
	void	CheckForValidOperaWindowBelowCursor();

#endif	//	this file
