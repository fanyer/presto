/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/* NOTE!
** This is the old geolocation PI interface. This interface is now
** 100% core-internal. No platforms should implement any of this.
*/

#ifndef MODULES_GEOLOCATION_GEOLOCATION_H
#define MODULES_GEOLOCATION_GEOLOCATION_H

#ifdef GEOLOCATION_SUPPORT

class OpGeolocationListener;

class OpGeolocation
{
public:
	/** @short Bitfield specifying the different methods than can be used to find the users positon. */
	enum Type
	{
		/* Position can be/was obtained through whatever provider was available. */
		ANY = -1,

		/* Position should be/was obtained through GPS */
		GPS = 0x01,		

		/* Position was should be/was obtained through WIFI triangulation */
		WIFI = 0x02,

		/* Position was should be/was obtained through cell tower triangulation */
		RADIO = 0x04
	};

	/** @short Object holding geograpical location and possibly more. */
	struct Position
	{
		/** @short Bitfield describing which fields in this class are valid. */
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

		/** Bitmask of Capabilities specifying which fields are valid */
		int capabilities;

		/** The time the geographic location was aquired in milliseconds since 1970-01-01 00:00 UTC (GMT) (MANDATORY) */
		double timestamp;

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

		/** Bitmask of Type specifying what data was used to measure this location. */
		int type;

		/** Depending on what is specified in Position::type, one or more of the
		  * following may contain data. (Currently we only have additional data for radio)
		  */
		struct
		{
			struct {
				/** Cell ID (Cell ID (CID) for GSM and Base Station ID (BID) for CDMA). */
				int cell_id;
			} radio;
		} type_info;

		OP_STATUS CopyFrom(struct OpGpsData &source, double source_timestamp);
	};

	/** @short Request options to use with StartReception(). */
	struct Options
	{
		Options() :
			one_shot(FALSE),
			high_accuracy(FALSE),
			timeout(LONG_MAX),
			maximum_age(0),
			type (ANY) { }

		/** One shot or continuous.
		 *
		 * If one_shot == TRUE we only want one set of coordinates,
		 * if it's FALSE we're in continuous mode and want subsequent
		 * calls to OnGeolocationUpdate.
		 */
		BOOL one_shot;

		/** If TRUE, prefer the results as accurate as possible, even if this
		 * means that the request will take longer. */
		BOOL high_accuracy;

		/** Timeout value for initial reception, in milliseconds. */
		long timeout;

		/* Used for caching purposes. Return chached position if it's age
		 * is no more that maximum_age milliseconds old. If maximum_age ==
		 * infinity, return cached position regardless of age, OR return new
		 * position if there is no cached position.
		 */
		long maximum_age;

		/* Used to specify that this request should be resolved only via
		 * the specified type of provider.
		 *
		 * default: ANY
		 */
		int type;
	};

	/** @short Possible errors that are interesting for core. */
	struct Error
	{
		enum ErrorCode
		{
			/** No error */
			ERROR_NONE,

			/** Generic error */
			GENERIC_ERROR,

			/** Request was denied by user, or something similar */
			PERMISSION_ERROR,

			/** Request failed. GPS receiver returned error or is disabled or similar */
			PROVIDER_ERROR,

			/** The position of the device could not be determined */
			POSITION_NOT_FOUND,

			/** Request timed out */
			TIMEOUT_ERROR
		};

		/* Type of error
		 */
		ErrorCode error;

		/* Used to specify what provider caused this error.
		 *
		 * default: ANY
		 */
		int type;

		Error(ErrorCode error, int type)
			: error(error)
			, type(type)
		{ }
	};

	/** @short Destructor.
	 *
	 * Cancels the ongoing request, if any. No listener method calls should be
	 * made from the destructor.
	 */
	virtual ~OpGeolocation() { }

	/** @short Start reception of geographical position.
	 *
	 * The core code will asynchronously retrieve new coordinates from the
	 * location provider. Could be WIFI triangulation, GSM triangulation or anything.
	 * The result will be posted back to core by calling
	 * listener->OnGeolocationUpdate() in intervals as specified by
	 * options.update_interval.
	 *
	 * Only one operation at a time is allowed per listener; if a previous call to
	 * StartReception() has not completeid or has not yet been cancelled by StopReception()
	 * previous operation will now be autmatically stopped and replaced by this
	 * one.
	 *
	 * If an error occurs, listener->OnGeolocationError() is called.
	 * Subsequent calls to OnGeolocationError() are allowed. If the request
	 * times out, as defined by options.timeout,
	 * OnGeolocationError(GEOLOCATION_TIMEOUT_ERROR) is called.
	 *
	 * It's okay to ignore the return code from this function. The listener
	 * will be called regardless.
	 *
	 * @param options the requested options for this aqusition. See Options for
	 * details.
	 * @param listener Listener for this operation
	 * @return OK on success, ERR_NO_MEMORY on OOM and ERR on other errors.
	 */
	virtual OP_STATUS StartReception(const Options &options, OpGeolocationListener *listener) = 0;

	/** @short Stop continuous reception of geographical position.
	 *
	 * @param listener Listener for this operation
	 */
	virtual void StopReception(OpGeolocationListener *listener) = 0;

	/** @short Returns current ready state of the geolocation implementation.
	  *
	  * If the next call to StartReception is going to fail this function will
	  * return FALSE. This could happen if there are no data providers available
	  * to feed the geolocation core.
	  * 
	  * @return TRUE if ready, FALSE if not.
	  */
	virtual BOOL IsReady() = 0;

	/** @short Returns current state of geolocation availability.
	  *
	  * @return TRUE if geolocation is enabled, FALSE if not.
	  */
	virtual BOOL IsEnabled() = 0;

	/** @short Returns TRUE if the listener is in use, FALSE if not. 
	  * 
	  * @param listener The listener in question.
	  */
	virtual BOOL IsListenerInUse(OpGeolocationListener *listener) = 0;
};

/** @short Geolocation listener.
 *
 * Methods will be called as a response to a continuous reception of
 * geographical position issued by OpGeolocation::ReceivePosition().
 *
 * An object of this type serves two purposes:
 *
 *   - report location updates to to core
 *   - report errors to core
 */
class OpGeolocationListener
{
public:

	virtual ~OpGeolocationListener() { }

	/** @short New geographical location is available.
	 *
	 * When core has successfully received a new position from the
	 * positioning device, this method is called with the new position.
	 *
	 * @param position The new geographical location
	 */
	virtual void OnGeolocationUpdated(const OpGeolocation::Position &position) = 0;

	/** @short Geographical location reception failed.
	 *
	 * This will cancel the ongoing continuous reception of geographical
	 * position previously requested by OpGeolocation::StartReception().
	 *
	 * @param error Type of error that occurred.
	 */
	virtual void OnGeolocationError(const OpGeolocation::Error &error) = 0;
};

#endif // GEOLOCATION_SUPPORT

#endif // MODULES_GEOLOCATION_GEOLOCATION_H
