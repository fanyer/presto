/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef API_DEVICE_API_DEVICE_STATE_INFO_H
#define API_DEVICE_API_DEVICE_STATE_INFO_H

#ifdef PI_DEVICESTATEINFO

/** @short Provides information about device state.*/

class OpDeviceStateInfo
{
public:
	virtual ~OpDeviceStateInfo() { }

	/** Creates and returns OpDeviceStateInfo object.
	 *
	 * The OpDeviceStateInfo is a singleton class. The PiModule calls OpDeviceStateInfo::Create()
	 * when initializing Opera and deletes the singleton instance when terminating Opera.
	 *
	 * @param new_device_state_info (output) Set to the newly created OpDeviceStateInfo object.
	 * @return OK, ERR_NOT_SUPPORTED, ERR_NO_MEMORY, ERR
	 */
	static OP_STATUS Create(OpDeviceStateInfo** new_device_state_info);

	/** Enumeration of possible audio outputs that can be returned by GetAudioOutputDevice */
	enum AudioOutputDevice
	{
		/** No audio */
		AUDIO_DEVICE_NONE			=	0,

		/** Handset */
		AUDIO_DEVICE_HANDSET,

		/** Built-in speaker (loudspeaker) */
		AUDIO_DEVICE_SPEAKER,

		/** Headphones */
		AUDIO_DEVICE_HEADPHONES,

		/** Bluetooth connection */
		AUDIO_DEVICE_BLUETOOTH,

		/** USB connection */
		AUDIO_DEVICE_USB,

		/** Unknown device */
		AUDIO_DEVICE_UNKNOWN
	};

	/** Gets where audio is currently configured for output.
	 *
	 * @param[out] output Audio output device
	 * @return OK, ERR_NOT_SUPPORTED, ERR_NO_MEMORY, ERR
	 */
	virtual OP_STATUS GetAudioOutputDevice(AudioOutputDevice* output) const = 0;

	/** Checks device's screen backlight status.
	 *
	 * @return
	 *  - OpBoolean::IS_TRUE or OpBoolean::IS_FALSE it case of success
	 *  - OpStatus::ERR_NO_MEMORY if there is not enough memory to determine the backlight status
	 *  - OpStatus::ERR_NOT_SUPPORTED if backlight status is not available
	 *  - OpStatus::ERR in case of any other error.
	 */
	virtual OP_BOOLEAN IsBacklightOn() const = 0;

	/** Checks device's keypad backlight status.
	 *
	 * @return
	 *  - OpBoolean::IS_TRUE or OpBoolean::IS_FALSE it case of success
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR in case of unsepcified, unrecoverable error
	 */
	virtual OP_BOOLEAN IsKeypadBacklightOn() const = 0;
};

#endif // PI_DEVICESTATEINFO

#endif // API_DEVICE_API_DEVICE_STATE_INFO_H
