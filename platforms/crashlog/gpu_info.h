// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2011 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//


#ifndef GPU_INFO_H
#define GPU_INFO_H

struct GpuInfo
{
	static const unsigned int MAX_INFO_LENGTH = 4096; // Max length in bytes.

	// These values are chosen instead of simply 0 and 1 because we have to distinguish them from uninitialized memory.
	static const unsigned int NOT_INITIALIZED = 0xDEADC0de;
	static const unsigned int INITIALIZED = 0x0000f00d;

	GpuInfo() : initialized(NOT_INITIALIZED), device_info_size(0) {}

	unsigned int initialized;
	VEGA3dDevice* initialized_device;
	unsigned int device_info_size;
	char device_info[MAX_INFO_LENGTH];
};

// Pass in the current device, and the function will fill in the device info
// and set the "initialized" member to INITIALIZED to mark it as initialized.
void FillGpuInfo(VEGA3dDevice* device);

void DeviceDeleted(VEGA3dDevice* device);

#endif // GPU_INFO_H