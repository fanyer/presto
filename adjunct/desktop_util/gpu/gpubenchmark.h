/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef GPU_BENCHMARK_H
#define GPU_BENCHMARK_H

#ifdef VEGA_3DDEVICE

#include "platforms/vega_backends/vega_blocklist_device.h"

OP_STATUS TestGPU(unsigned int backend);

/**Retrieve performance/stability information of the hardware identified
 * uniquely by the identifier
 *
 * @param identifier used as a key to store relevant information about a
 *        device, platform is free to decide the format of the value, as
 *        long as it uniquely identify a device/driver/etc combination.
 *        On Windows it's something like "Win32 OpenGL [device_id] [device_version]"
 *
 * @return Yes: tested and usable, No: tested not usable, Maybe: not tested
 */
BOOL3 GetBackendStatus(OpStringC identifier);

OP_STATUS WriteBackendStatus(OpString identifier, bool enabled);

#endif // VEGA_3DDEVICE
#endif // GPU_BENCHMARK_H
