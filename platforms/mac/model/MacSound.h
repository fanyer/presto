#ifndef _MAC_OP_SOUND_H_
#define _MAC_OP_SOUND_H_

#ifndef NO_CARBON

#include "platforms/mac/model/macthread.h"
#include "modules/util/simset.h"
#define Size MacSize
#define Style MacFontStyle
#include <QuickTime/QuickTime.h>
#undef Style
#undef Size

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

typedef struct {
	AudioFormatAtom		formatData;
	AudioEndianAtom		endianData;
	AudioTerminatorAtom	terminatorData;
} AudioCompressionAtom, *AudioCompressionAtomPtr, **AudioCompressionAtomHandle;

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif

typedef struct {
	ExtendedSoundComponentData	compData;
	Handle						hSource;			// source media buffer
	Media						sourceMedia;		// sound media identifier
	TimeValue					getMediaAtThisTime;
	TimeValue					sourceDuration;
	UInt32						maxBufferSize;
	Boolean						isThereMoreSource;
	Boolean						isSourceVBR;
} SCFillBufferData, *SCFillBufferDataPtr;

enum eBufferNumber { kFirstBuffer, kSecondBuffer };

class MacOpSound : protected MacThread, public Link
{
public:
	virtual ~MacOpSound();

	OP_STATUS 	Init();
	static void Terminate();

	OP_STATUS 	Play(BOOL async);
	OP_STATUS 	Stop();

	static MacOpSound* GetSound(const uni_char* fullpath);

private:
	FSSpec			m_SoundFile;
	MPSemaphoreID 	m_BufferDoneSignalID;
	MPSemaphoreID 	m_PlayingStoppedSignalID;
	SndChannelPtr	m_UsedChannel;
	Boolean			m_StopPlaying;

	MacOpSound(FSSpec *spec);
	virtual OSStatus run();

	static MacOpSound* FindSoundForChannel(SndChannelPtr theChannel);
	static pascal void OpSoundCallBackFunction(SndChannelPtr theChannel, SndCommand *theCmd);
	static pascal Boolean SoundConverterFillBufferDataProc(SoundComponentDataPtr *outData, void *inRefCon);
	static Boolean isQTinitialized;
	static CGrafPtr suppressVideoOffscreen;
	static Head theSounds;
};

#endif // NO_CARBON

#endif // _MAC_OP_SOUND_H_
