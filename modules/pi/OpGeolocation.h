/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_PI_OPGEOLOCATION_H
#define MODULES_PI_OPGEOLOCATION_H

#ifdef PI_GEOLOCATION

/** @file
  * Porting interfaces for geolocation
  *
  * Core will try to instantiate one instance of each of the following
  * objects OpGeolocationWifiDataProvider, OpGeolocationRadioDataProvider
  * and OpGeolocationGpsDataProvider.
  *
  * If one or more of these are not supported by the platform it should
  * return OpStatus::ERR_NOT_SUPPORTED in the corresponding Create function.
  *
  * When core wants data from the platform it will call Poll() on these objects
  * and expect the platform to return immediately. The corresponding
  * OnNewDataAvailable must be called in total one time per call to Poll(). This
  * may be happen before Poll() returns, if data collection is more or less
  * immediate. Otherwise, the platform needs to break this into its own thread
  * etc. and then feed this back to core on the core thread.
  *
  * If Poll did not return an error it's expected that OnNewDataAvailable will
  * be called.
  * If there is an error so that OnNewDataAvailable can not return useful data,
  * it must be called with a NULL pointer (of the corresponding type). This is
  * important to keep the caller (the geolocation module) in a sane state.
  *
  * The core listener is a singleton, and will live for the duration of the
  * core object.
  *
  * When core receives the data, the data is timestamped by core, so it's
  * important that the collected data is as fresh as possible.
  *
  */

/** OpGeolocationDataProviderBase
  *
  * This is the base class for all the data provider classes.
  *
  * Currently there's only one function, Poll() which will be called
  * by core when it requires new data.
  */
class OpGeolocationDataProviderBase
{
public:
	virtual ~OpGeolocationDataProviderBase() {}

	/* Collect data.
	 *
	 * Fill in the corresponding Op*Data structure and invoke the callback.
	 *
	 * The platform implementation is then expected to return immediately,
	 * and call the appropriate callback on OpGeolocationDataListener when
	 * the requested data is available.
	 *
	 * If the data collection is immediate, it's okay to invoke the callback
	 * synchronously.
	 *
	 * @result OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS Poll() = 0;
};

class OpGeolocationWifiDataProvider : public OpGeolocationDataProviderBase
{
public:
	virtual ~OpGeolocationWifiDataProvider() {}

	/** Create an OpGeolocationWifiDataProvider object.
	  *
	  * This provider will be used by the core geolocation code to retrieve
	  * information about surrounding wifi access points.
	  *
	  * @param new_obj The newly created object.
	  * @param listener The listener that the platform will inform about new data
	  * @result OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM or OpStatus::ERR_NOT_SUPPORTED if the data type is unsupported.
	  */
	static OP_STATUS Create(OpGeolocationWifiDataProvider *& new_obj, class OpGeolocationDataListener *listener);
};

class OpGeolocationRadioDataProvider : public OpGeolocationDataProviderBase
{
public:
	virtual ~OpGeolocationRadioDataProvider() {}

	/** Create an OpGeolocationRadioDataProvider object.
	  *
	  * This provider will be used by the core geolocation code to retrieve
	  * information about cell phone status and surrounding cell phone towers.
	  *
	  * @param new_obj The newly created object.
	  * @param listener The listener that the platform will inform about new data
	  * @result OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM or OpStatus::ERR_NOT_SUPPORTED if the data type is unsupported.
	  */
	static OP_STATUS Create(OpGeolocationRadioDataProvider *& new_obj, class OpGeolocationDataListener *listener);
};

class OpGeolocationGpsDataProvider : public OpGeolocationDataProviderBase
{
public:
	virtual ~OpGeolocationGpsDataProvider() {}

	/** Create an OpGeolocationGpsDataProvider object.
	  *
	  * This provider will be used by the core geolocation code to retrieve
	  * geographical coordinates from a GPS device.
	  *
	  * @param new_obj The newly created object.
	  * @param listener The listener that the platform will inform about new data
	  * @result OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM or OpStatus::ERR_NOT_SUPPORTED if the data type is unsupported.
	  */
	static OP_STATUS Create(OpGeolocationGpsDataProvider *& new_obj, class OpGeolocationDataListener *listener);
};

/** @short Structure holding information about wifi access points. */
struct OpWifiData
{
	struct AccessPointData
	{
		/* The MAC address of the wifi node. (MANDATORY)
		 *
		 * Expected format is %02X-%02X-%02X-%02X-%02X-%02X
		 * Example: 00-21-29-93-52-7B
		 */
		OpString mac_address;

		/* The SSID of the wifi node. (MANDATORY)
		 * Example: wiifi
		 */
		OpString ssid;

		/* Current signal strength measured in dBm. (MANDATORY)
		 * Example: -88
		 */
		INT32 signal_strength;

		/* Channel.
		 * (Optional, 0 means there is no value)
		 */
		BYTE channel;

		/* Current signal to noise ratio, in dB.
		 * (Optional, 0 means there is no value)
		 */
		INT16 snr;
	};

	/* List of found wifi access points.
	 * Can be empty.
	 */
	OpAutoVector<AccessPointData> wifi_towers;

	/* Helper function to copy contents of OpWifiData.
	 */
	OP_STATUS CopyTo(OpWifiData &destination);

	/* Helper function to move contents of OpWifiData.
	 */
	OP_STATUS MoveTo(OpWifiData &destination);
};

/** @short Structure holding information about radio (cell) towers. */
struct OpRadioData
{
	struct CellData
	{
		/* Unique identifier of the cell.
		 * CID for GSM and BID for CDMA.
		 */
		INT32 cell_id;

		/* Location Area Code.
		 * LAC for GSM and NID for CDMA.
		 */
		UINT16 location_area_code;

		/* Mobile Country Code.
		 * MCC for GSM and CDMA.
		 */
		INT16 mobile_network_code;

		/* Mobile Network Code.
		 * MNC for GSM and SID for CDMA.
		 */
		INT16 mobile_country_code;

		/* Radio signal strength measured in dBm.
		 */
		INT16 signal_strength;

		/* Represents the distance from the cell tower.
		 * Each unit is roughly 550 meters. (0-63)
		 */
		BYTE timing_advance;
	};

	/** @short Bitfield describing what type of cell tower this is. */
	typedef enum
	{
		/** Radio type is not known. */
		RADIO_TYPE_UNKNOWN,

		/** Radio type is GSM. */
		RADIO_TYPE_GSM,

		/** Radio type is CDMA. */
		RADIO_TYPE_CDMA,

		/** Radio type is WCDMA. */
		RADIO_TYPE_WCDMA
	} RadioType;

	/* What type of cell tower this is.
	 * One of the values specified in the RadioType enum.
	 */
	RadioType radio_type;

	/* Carrier name.
	 * Used to help disambiguate MCC/MNC
	 */
	OpString carrier;

	/* The mobile network code for the device's home network.
	 */
	INT32 home_mobile_network_code;

	/* The mobile country code for the device's home network
	 */
	INT32 home_mobile_country_code;

	/* List of found cell towers.
	 * Can be empty.
	 */
	OpAutoVector<CellData> cell_towers;

	/* Cell ID of the primary cell tower. This id should be
	 * present in the cell_towers list.
	 * Set to -1 if not available
	 */
	INT32 primary_cell_id;

	/* Helper function to copy contents of OpRadioData.
	 */
	OP_STATUS CopyTo(OpRadioData &destination);

	/* Helper function to move contents of OpRadioData.
	 */
	OP_STATUS MoveTo(OpRadioData &destination);
};

/** @short Structure holding information about a measured gps position. */
struct OpGpsData
{
	/** @short Bitfield describing which fields in this structure are valid. */
	enum Capabilities
	{
		/** Only the mandatory fields are supplied. */
		STANDARD = 0x00,

		/** 'altitude' field is valid */
		HAS_ALTITUDE = 0x01,

		/** 'altitude_accuracy' field is valid */
		HAS_ALTITUDEACCURACY = 0x02,

		/** 'heading' field is valid */
		HAS_HEADING = 0x04,

		/** 'velocity' field is valid */
		HAS_VELOCITY = 0x08
	};

	/** Bitmask of Capabilities specifying which fields are valid (MANDATORY) */
	int capabilities;

	/** Latitude in degrees (MANDATORY) */
	double latitude;

	/** Longitude in degrees (MANDATORY) */
	double longitude;

	/** Latitude and longitude accuracy in meters (MANDATORY) */
	double horizontal_accuracy;

	/** Meters above sea level */
	double altitude;

	/** Altitude accuracy in meters */
	double vertical_accuracy;

	/** Direction in degrees counting clockwise from true north */
	double heading;

	/** Ground speed in meters per second */
	double velocity;

	/* Helper function to copy contents of OpGpsData.
	 */
	OP_STATUS CopyTo(OpGpsData &destination);
};

/** OpGeolocationDataListener
  *
  * This is the listener that is implemented by core.
  *
  * After the platform has received a Poll() call on
  * one of the data provider classes, it should call
  * the corresponding OnNewDataAvailable function when
  * the data is ready for consumption by core.
  *
  * Core copies this data, thus the platform retains
  * ownership of the object passed as argument to these
  * functions.
  */
class OpGeolocationDataListener
{
public:
	/** Call this when new wifi data is available.
	 *
	 * @param data Points to a structure containing the new data.
	 * The data will be copied by OnNewDataAvailable.
	 * May be NULL to indicate an error.
	 */
	virtual void OnNewDataAvailable(OpWifiData *data) = 0;

	/** Call this when new radio data is available.
	 *
	 * @param data Points to a structure containing the new data.
	 * The data will be copied by OnNewDataAvailable.
	 * May be NULL to indicate an error.
	 */
	virtual void OnNewDataAvailable(OpRadioData *data) = 0;

	/** Call this when new GPS data is available.
	 *
	 * @param data Points to a structure containing the new data.
	 * The data will be copied by OnNewDataAvailable.
	 * May be NULL to indicate an error.
	 */
	virtual void OnNewDataAvailable(OpGpsData *data) = 0;
};

#endif // PI_GEOLOCATION
#endif // MODULES_PI_OPGEOLOCATION_H
