/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PI_DEVICE_API_OPRADIOINFO_H
#define PI_DEVICE_API_OPRADIOINFO_H

#ifdef PI_TELEPHONYNETWORKINFO

class OpRadioSourceListener;

/** @short Provides information about radio telephony network.
 *
 * Unless otherwise stated, all the data provided by this class is related
 * to the network the device is currently connected to (multi-network devices,
 * e.g. phones with two SIM cards, are not supported).
 */
class OpTelephonyNetworkInfo
{
public:
	virtual ~OpTelephonyNetworkInfo() { }

	static OP_STATUS Create(OpTelephonyNetworkInfo** new_telephony_network_info);

	/** Checks whether radio is enabled (e.g. in flight mode it is disabled).
	 *
	 * @return
	 *  - OpBoolean::IS_TRUE or OpBoolean::IS_FALSE it case of success
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR in case of unsepcified, unrecoverable error
	 */
	virtual OP_BOOLEAN IsRadioEnabled() const = 0;

	/** Checks whether the device is connected to a different network than the home network.
	 *
	 * The device is roaming if either of MCC and MNC of the currently
	 * connected network is different than the values for home network
	 * indicated on the SIM card.
	 *
	 * @return
	 *  - OpBoolean::IS_TRUE or OpBoolean::IS_FALSE it case of success
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR in case of unsepcified, unrecoverable error
	 */
	virtual OP_BOOLEAN IsRoaming() const = 0;

	/** Gets radio signal strength.
	 *
	 * @param[out] signal_strength Numeric value of siganl strength. The
	 * values range form 0.0 to 1.0 - where 1.0 means maximum radio strength
	 * and 0.0 means no radio signal.
	 *
	 * @return OK, ERR_NOT_SUPPORTED, ERR_NO_MEMORY, ERR
	 */
	virtual OP_STATUS GetRadioSignalStrength(double* signal_strength) const = 0;

	enum RadioSignalSource
	{
		RADIO_SIGNAL_GSM,
		RADIO_SIGNAL_LTE,
		RADIO_SIGNAL_CDMA,
		RADIO_SIGNAL_WCDMA,
		RADIO_SIGNAL_TDSCDMA // ...
	};

	enum RadioDataBearer
	{
		RADIO_DATA_CSD,
		RADIO_DATA_GPRS,
		RADIO_DATA_EDGE,
		RADIO_DATA_HSCSD,
		RADIO_DATA_EVDO,
		RADIO_DATA_LTE,
		RADIO_DATA_UMTS,
		RADIO_DATA_HSPA ,
		RADIO_DATA_UNKNOWN,
		RADIO_DATA_ONEXRTT //...
	};

	/** Gets radio signal source
	 *
	 * @param[out] source Source (or type) of network connection.
	 * @return OK, ERR_NOT_SUPPORTED, ERR_NO_MEMORY, ERR
	 */
	virtual OP_STATUS GetRadioSignalSource(RadioSignalSource* source) const = 0;

	/** Gets radio data bearer
	 *
	 * @param[out] bearer Bearer of data communication
	 * @return OK, ERR_NOT_SUPPORTED, ERR_NO_MEMORY, ERR
	 */
	virtual OP_STATUS GetRadioDataBearer(RadioDataBearer* bearer) const = 0;

	/** Appends a RadioSourceListener to the listener list.
	 *
	 * The listener's OnRadioSourceChanged method will be called whenever
	 * radio source changes and OnRadioDataBearerChanged whenever data bearer
	 * changes.
	 *
	 * Any listener objects that are still registered when this object is being
	 * destroyed are also deleted.
	 *
	 * @param listener Listener to be added to the listener list.
	 *
	 * @return
	 *  - OK in case of success
	 *  - ERR if the listener has already been registered
	 *  - ERR_NO_MEMORY
	 */
	virtual OP_STATUS AddRadioSourceListener(OpRadioSourceListener* listener) = 0;

	/** Removes RadioSourceListener from listener list.
	 *
	 * If the listener is not in the list of listeners or NULL then platform
	 * should ignore it (though triggering OP_ASSERT is fine since it probably
	 * is a symptom of programmer error).
	 *
	 * @param listener Listener to remove from the listener list.
	 */
	virtual void RemoveRadioSourceListener(OpRadioSourceListener* listener) = 0;
};

/** @short Listener that is notified if radio signal source changes.
 */
class OpRadioSourceListener
{
public:
	virtual ~OpRadioSourceListener() { }
	/**
	 * Callback method to invoke when the signal source or roaming state changes
	 *
	 * @param radio_source - currently used radio source.
	 * @param is_roaming - set to true if we are roaming. By roaming here we understand
	 *        that user is not using his operators home network.
	 */
	virtual void OnRadioSourceChanged(OpTelephonyNetworkInfo::RadioSignalSource radio_source, BOOL is_roaming) = 0;
	/**
	 * Callback method to invoke when the radio data bearer changes
	 *
	 * @param radio_source - currently used radio data bearer.
	 */
	virtual void OnRadioDataBearerChanged(OpTelephonyNetworkInfo::RadioDataBearer radio_source) = 0;
};

#endif // PI_TELEPHONYNETWORKINFO

#endif // PI_DEVICE_API_OPRADIOINFO_H
