#if !defined(MACOPMESSAGEHANDLER_H)
#define MACOPMESSAGEHANDLER_H

enum
{
	kEventClassOperaPlatformIndependent = 'OpPi'
};

enum
{
	kEventOperaMessage = 1
};

enum
{
	kEventParamOperaMessage = 'OMSG',//FOUR_CHAR_CODE('OMSG'),
	kEventParamOperaMessageData = 'OMSD', //FOUR_CHAR_CODE('OMSD'),
	kEventParamOperaMessageParam1 = 'OMP1', //FOUR_CHAR_CODE('OMP1'),
	kEventParamOperaMessageParam2 = 'OMP2' //FOUR_CHAR_CODE('OMP2')
};

class MacOpMessageLoop //: public OpMessageLoop
{
public:
	MacOpMessageLoop();
	virtual ~MacOpMessageLoop();

	OP_STATUS Init();

	virtual OP_STATUS Run();

#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
	virtual OP_STATUS RunSlice();
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

	virtual OP_STATUS PostMessage(UINT32 msg, void *data, UINT32 param1 = 0, UINT32 param2 = 0);

	virtual void DispatchAllPostedMessagesNow();

	OSStatus MessageLoopHandler(EventRef inEvent);
	static pascal OSStatus sMacMessageLoopHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

private:
	EventHandlerRef	mHandlerRef;
	EventHandlerUPP mHandlerUPP;
};

#endif
