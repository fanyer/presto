#ifndef MacOpScreenInfo_H
#define MacOpScreenInfo_H

#include "modules/pi/OpScreenInfo.h"

class MacOpScreenInfo : public OpScreenInfo
{
public:
	virtual ~MacOpScreenInfo() {}

	/** Get properties from a screen based on what window and what pixel */
	virtual OP_STATUS GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point = NULL);

	/** Get the DPI propertis only (superfast)*/
	virtual OP_STATUS GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point = NULL);

	/** Makes it simple to register 'main-screen' fallback properties */
	virtual OP_STATUS RegisterMainScreenProperties(OpScreenProperties& properties);

	/** Returns the amount of memory a bitmap of the specified type would need */
	virtual UINT32 GetBitmapAllocationSize(UINT32 width,
											UINT32 height,
											BOOL transparent,
											BOOL alpha,
											INT32 indexed);

	void * GetScreen(OpWindow* window, const OpPoint* point);

public:
//---------------------------------------------------------------------
// OLD AND DEPRECATED
//---------------------------------------------------------------------

};

#endif //MacOpScreenInfo_H
