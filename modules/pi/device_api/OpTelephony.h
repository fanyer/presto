/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_DEVICE_API_OPTELEPHONY_H
#define PI_DEVICE_API_OPTELEPHONY_H

#ifdef PI_TELEPHONY

typedef OP_STATUS OP_TELEPHONYSTATUS;

class OpTelephonyListener;

/** Object for manipulation on call log. */
class OpTelephony
{
public:
	class Status : public ::OpStatus
	{
	public:
		enum
		{
			ERR_WRONG_CALL_RECORD_TYPE = USER_ERROR + 1,
			ERR_CALL_RECORD_PARAMETER,
			ERR_ID_NOT_FOUND
		};
	};

	struct CallRecord
	{
		enum Type
		{
			Received,
			Initiated,
			Missed,
			TypeUnknown
		};

		CallRecord()
			: m_type(TypeUnknown)
			, m_duration(0)
			, m_start(0)
		{}

		/** Copy contents of the CallRecord.  Implemented by core.
		 *
		 * @param[out] new_call_record Placeholder for the copy of the CallRecord.
		 *
		 * @return
		 *		OpStatus::OK,
		 *		OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS CopyTo(CallRecord** new_call_record);

		/** Phone number of the other party of the call. */
		OpString m_phone_number;

		/** Identifier for the CallRecord. It is unique regardless of m_type. */
		OpString m_id;

		/** Name, taken from address book, of the other party
		 * of the call, connected with phone_number.
		 *
		 * If there is no such entry in address book, then name is empty.
		 *
		 * If there are several address book items connected with given
		 * phone number, it's up to platfrom which one to choose.
		 */
		OpString m_name;

		/** Type of the CallRecord. */
		Type m_type;

		/** Duriation of the call in seconds. */
		unsigned int m_duration;

		/** Start time of the call.
		 * Has absolute date format in seconds since Jan 1, 1970 UTC.
		 */
		time_t m_start;
	};

	/** @short A callback to call after operation has ended.
	 * To be used, when no return information other then error code is needed.
	 */
	class SimpleCallback
	{
	public:
		/** Called when operation failed.
		 *
		 * @param[in] error See OP_TELEPHONYSTATUS.
		 */
		virtual void OnFailed(OP_TELEPHONYSTATUS error) = 0;

		/** Called when operation succeeds. */
		virtual void OnFinished() = 0;
	};

	/** @short A callback to call after GetCallRecord has ended. */
	class GetCallRecordCallback
	{
	public:
		/** Called when operation failed.
		 *
		 * @param[in] error See OP_TELEPHONYSTATUS.
		 */
		virtual void OnFailed(OP_TELEPHONYSTATUS error) = 0;

		/** Called when operation succeeds.
		 *
		 * @param[in] result Owned by the caller.
		 */
		virtual void OnFinished(const CallRecord* result) = 0;
	};

	/** @short A callback to call after GetCallRecordCount has ended. */
	class GetCallRecordCountCallback
	{
	public:
		/** Called when operation failed.
		 *
		 * @param[in] error See OP_TELEPHONYSTATUS.
		 */
		virtual void OnFailed(OP_TELEPHONYSTATUS error) = 0;

		/** Called when operation succeeds. */
		virtual void OnFinished(unsigned int result) = 0;
	};

	/** @short A callback to call for every CallRecord found with specified filter.  Implemented by core. */
	class CallRecordsSearchIterationCallback
	{
	public:
		/** Called when find operation has a result ready.
		 *
		 * @param[in] result Result of a search, allocated by platform
		 *		with OP_NEW.  Passes ownership to the callback.
		 *
		 * @return
		 *  - TRUE if the callback wants more results.
		 *  - FALSE if the callback wants to stop the search.  In such case
		 *		this callback MUST NOT be called back again.
		 */
		virtual BOOL OnItem(CallRecord* result) = 0;

		/** Called when find operation finishes successfully. */
		virtual void OnFinished() = 0;

		/** Called when the find operation fails.
		 *
		 * @param[in] error See OP_TELEPHONYSTATUS.
		 */
		virtual void OnError(OP_TELEPHONYSTATUS error) = 0;
	};

	/** Constructs OpTelephony.
	 *
	 * @param[out] new_telephony Placeholder for the newly created messaging
	 *		object.  Caller should clean messaging up if done with it.
	 * @param[in] listener See OpTelephonyListener.
	 *
	 * @return
	 *		OpStatus::OK,
	 *		OpStatus::ERR_NULL_POINTER - if new_telephony is NULL,
	 *		OpStatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpTelephony** new_telephony, OpTelephonyListener* listener);

	/** @short Delete one call log entry.
	 *
	 * @param[in] type Type of to-be-deleted CallRecord.
	 *		If the type is TypeUnknown, this method returns
	 *		OP_TELEPHONYSTATUS::ERR_WRONG_CALL_RECORD_TYPE.
	 * @param[in] id See OpTelephony::CallRecord::m_id.
	 * @param[in] callback This object will be called upon finishing of
	 *		deleting CallRecord operation.  Methods on the callback
	 *		may be called synchronously from the method (for example
	 *		to immediately report an error).  See OP_TELEPHONYSTATUS
	 *		and OP_STATUS.
	 */
	virtual void DeleteCallRecord(CallRecord::Type type, const uni_char* id, SimpleCallback* callback) = 0;

	/** @short Delete all call records of given type.
	 *
	 * @param[in] type Type of to-be-deleted CallRecords.
	 *		If the type is TypeUnknown, this method returns
	 *		OP_TELEPHONYSTATUS::ERR_WRONG_CALL_RECORD_TYPE.
	 * @param[in] callback This object will be called upon finishing of
	 *		deleting all CallRecords operation.  Methods on the callback
	 *		may be called synchronously from the method (for example
	 *		to immediately report an error).  See OP_TELEPHONYSTATUS
	 *		and OP_STATUS.
	 */
	virtual void DeleteAllCallRecords(CallRecord::Type type, SimpleCallback* callback) = 0;

	/** @short Retrieve information about single call log entry.
	 *
	 * @param[in] type Type of requested CallRecord.
	 *		If the type is TypeUnknown, this method returns
	 *		OP_TELEPHONYSTATUS::ERR_WRONG_CALL_RECORD_TYPE.
	 * @param[in] id See OpTelephony::CallRecord::m_id.
	 * @param[in] callback This object will be called upon finishing
	 *		this operation.  Methods on the callback may be called
	 *		synchronously from the method (for example to immediately report
	 *		an error).  See OP_TELEPHONYSTATUS and OP_STATUS.
	 */
	virtual void GetCallRecord(CallRecord::Type type, const uni_char* id, GetCallRecordCallback* callback) = 0;

	/** @short Get number of call log entries of given type.
	 *
	 * @param[in] type Type of to-be-counted CallRecords.
	 *		If the type is TypeUnknown, this method returns
	 *		OP_TELEPHONYSTATUS::ERR_WRONG_CALL_RECORD_TYPE.
	 * @param[in] callback This object will be called upon finishing
	 *		this operation.  Methods on the callback may be called
	 *		synchronously from the method (for example to immediately report
	 *		an error).  See OP_TELEPHONYSTATUS and OP_STATUS.
	 */
	virtual void GetCallRecordCount(CallRecord::Type type, GetCallRecordCountCallback* callback) = 0;

	/** Calls given callback for each CallRecord of given type.
	 *
	 * Iteration should be ordered by CallRecord::m_id.
	 *
	 * @param[in] type The type of CallRecords which should be iterated.
	 *		If its value is TypeUnknown, then iteration is performed
	 *		on all types of CallRecords.
	 * @param[in] callback Object which is called when operation finishes
	 *		or fails. Can be called synchronously (during execution of this
	 *		function) or asynchronously. MUST NOT be NULL.
	 */
	virtual void Iterate(CallRecord::Type type, CallRecordsSearchIterationCallback* callback) = 0;

	virtual ~OpTelephony() {}
};

class OpTelephonyListener
{
public:
	/** Called when a call event happened.
	 *
	 * This method has to be called after the call event has ended.
	 *
	 * @param[in] type Type of the call event.  Cannot be TypeUnknown.
	 * @param[in] phone_number Phone number of the other party of the call.
	 */
	virtual void OnCall(OpTelephony::CallRecord::Type type, const OpString& phone_number) = 0;
};

#endif // PI_TELEPHONY

#endif // PI_DEVICE_API_OPTELEPHONY_H

