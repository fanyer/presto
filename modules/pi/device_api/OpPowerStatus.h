/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 **
 ** Copyright (C) 2010 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef PI_DEVICE_API_OPPOWERSTATUS_H
#define PI_DEVICE_API_OPPOWERSTATUS_H

#ifdef PI_POWER_STATUS

class OpPowerStatus;

/** @short Listener for power-related events.
*/
class OpPowerStatusListener
{
	public:
		virtual ~OpPowerStatusListener() { }

		/** Called when device wakes from sleep or suspend mode.
		 *
		 * @param power_status[in] The object which issued the notification.
		 */
		virtual void OnWakeUp(OpPowerStatus* power_status) = 0;

		/** Called when the battery charge level changes.
		 *
		 * It is up to the platform implementation to decide how often this function
		 * should be called. In particular it is not necessary that it is called
		 * whenever the charge level changes by one unit, it may be rarer than that
		 * (or more often but that would be a waste of resources in most cases).
		 *
		 * The function must be called when the battery becomes fully charged.
		 *
		 * @param power_status[in] The object which issued the notification.
		 * @param new_charge_level[in] The new value of battery charge level, 0 meaning
		 * that the battery is empty, 255 that it is full.
		 */
		virtual void OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level) = 0;

		/** Type of power source.
		*/
		enum PowerSupplyType
		{
			POWER_SOURCE_BATTERY,   ///< The device is powered from a battery.
			POWER_SOURCE_SOCKET     ///< The device is powered from an external power supply.
		};

		/** Called when the power source changes (e.g. a power cord is connected).
		 *
		 * @param power_status[in] The object which issued the notification.
		 * @param power_source[in] Type of the new power source.
		 */
		virtual void OnPowerSourceChange(OpPowerStatus* power_status, PowerSupplyType power_source) = 0;

		/** Called when the device enters or leaves low-power state.
		 *
		 * The low-power state is a state in which the device should conserve
		 * as much energy as possible. This state might be called sleep, deep sleep,
		 * low-power state, etc.
		 *
		 * Opera will do as little work as possible when in this state.
		 *
		 * @param power_status[in] The object which issued the notification.
		 * @param is_low[in] True when entering low power state, false when leaving.
		 */
		virtual void OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low) = 0;
};

/** This class listens to OpPowerStatus and sends incoming events to subscribers.
 *
 * OpPowerStatus sends its notifications to the monitor which in turn redirects
 * these notifications to OpPowerStatusListeners subscribed by AddListener.
 *
 * Implemented by core.
 */
class OpPowerStatusMonitor : public OpPowerStatusListener
{
	public:
		virtual ~OpPowerStatusMonitor();
		static OP_STATUS Create(OpPowerStatusMonitor** new_power_monitor);

		/** Add a listener.
		 *
		 * @param listener[in] The listener to be added.
		 * @return See OpStatus.
		 */
		virtual OP_STATUS AddListener(OpPowerStatusListener* listener);

		/** Remove a listener.
		 *
		 * A listener should not be removed during handling OpPowerStatus
		 * events.
		 *
		 * @param listener[in] The listener to be removed.
		 * @return
		 *  - OpStatus::OK - when successfully removed,
		 *  - OpStatus::ERR - when the listener is not valid (e.g. hasn't been
		 *		added.)
		 */
		virtual OP_STATUS RemoveListener(OpPowerStatusListener* listener);

		OpPowerStatus* GetPowerStatus() { return m_power_status; }

	/** Implementation of OpPowerStatusListener. */
	virtual void OnWakeUp(OpPowerStatus* power_status);
	virtual void OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level);
	virtual void OnPowerSourceChange(OpPowerStatus* power_status, PowerSupplyType power_source);
	virtual void OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low);
private:
	OpAutoVector<OpPowerStatusListener> m_listeners;
	OpPowerStatus* m_power_status;
};

/** @short Provides information on power status of the device.
*/
class OpPowerStatus
{
	public:
		virtual ~OpPowerStatus() { }

		/**
		 *  @param[out] new_power_status Newly created object.
		 *  @param[in] monitor Listener which sends OpPowerStatus events
		 *		to subscribers. See OpPowerStatusMonitor. Cannot be null.
		 */
		static OP_STATUS Create(OpPowerStatus** new_power_status, OpPowerStatusMonitor* monitor);

		/** Is the external power source connected or is the device running on battery?
		 *
		 * @param is_connected[out] Set to TRUE if power cord is connected,
		 * FALSE if running on battery.
		 *
		 * @return
		 *  - OpStatus::OK,
		 *  - OpStatus::ERR - generic error code.
		 */
		virtual OP_STATUS IsExternalPowerConnected(BOOL* is_connected) = 0;

		/** Get the battery charge as a value between 0 and 255, where 0 is a
		 *  completely flat (empty) battery, and 255 is a fully charged battery.
		 *
		 * @param charge[out] Battery charge as a value between 0 and 255.
		 *
		 * @return
		 *  - OpStatus::OK,
		 *  - OpStatus::ERR - generic error code.
		 */
		virtual OP_STATUS GetBatteryCharge(BYTE* charge) = 0;

		/** Is the device in a low-power state?
		 *
		 * This state might be called sleep, deep sleep, low-power state, etc.
		 * Opera will do as little work as possible when in this state.
		 *
		 * Some devices might not have this functionality, in which case they
		 * must always return OpStatus::OK and set is_in_low_power_state
		 * parameter to FALSE.
		 *
		 * @param is_in_low_power_state[out] Set to TRUE if the device
		 * is in a low power state, FALSE if it is in the regular state.
		 *
		 * @return
		 *  - OpStatus::OK,
		 *  - OpStatus::ERR - generic error code.
		 */
		virtual OP_STATUS IsLowPowerState(BOOL* is_in_low_power_state) = 0;
};

#endif // PI_POWER_STATUS
#endif // PI_DEVICE_API_OPPOWERSTATUS_H

