/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_DATA_DESCRIPTOR_H__
#define __OP_DATA_DESCRIPTOR_H__

#include "modules/cache/url_dd.h"
#include "modules/opdata/OpData.h"
#include "modules/opdata/UniString.h"

/** @class OpDataDescriptor
 *
 *  Use OpDataDescriptor to read the content of an OpResource. The same OpResource can be read
 *  in different ways depending on the parameters of the call to OpResource::GetDataDescriptor().
 *
 *  Usage:
 *  @code
 * 	OpURL url = OpURL::Make("http://t/core/networking/http/cache/data/blue.jpg");
 *  OpRequest *request;
 *  if (OpStatus::IsSuccess(OpRequest::Make(request, requestListener, url))
 *    request->SendRequest();
 *  @endcode
 *
 *  As a result of this you will get a callback to the request-listener containing the OpResponse object, from which
 *  you can start reading the data. For instance:
 *
 *  @code
 *  void MyListener::OnResponseFinished(OpRequest *request, OpResponse *response)
 *  {
 *    OpResource *resource = response->GetResource();
 *    OpDataDescriptor *dd;
 *    OpData data;
 *    if (resource &&
 *        OpStatus::IsSuccess(resource->GetDataDescriptor(dd)) &&
 *        OpStatus::IsSuccess(dd->RetrieveData(data)))
 *    {
 *    	... interpret data
 *    }
 *  }
 *  @endcode
 */

class OpDataDescriptor
{
public:
	virtual ~OpDataDescriptor();

	/** Transfer all available data from network/cache/file and append it to
	 *  the given buffer. This updates the current offset, and the same data
	 *  will not be returned again without calling SetPosition().
	 *
	 *  If RetrieveData is called in OnResponseDataLoaded, we are only handing
	 *  over existing data chunks, so the operation does not require much
	 *  memory (only for potential OpData support objects).
	 *
	 *	However, if RetrieveData is called in OnResponseFinished and the
	 *	resource is huge, it may be temporarily stored on disk. You should then
	 *	limit the amount of data retrieved to reduce the risk of OOM. If you
	 *	set a retrieve_limit, you should call RetrieveData repeatedly until no
	 *	more data is returned.
	 *
	 *  @param dst Destination buffer to append data to
	 *  @param retrieve_limit If not 0, limits the amount of bytes retrieved
	 *  @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS RetrieveData(OpData &dst, OpFileLength retrieve_limit = 0);

	/** Transfer all available data from network/cache/file and append it to
	 *  the given string. The content of the resource must be of unicode text
	 *  type. See the OpData variant of RetrieveData for details.
	 *
	 *  It is not allowed to mix this version of RetrieveData with the OpData
	 *  variant above. In particular, if an odd number of bytes have been
	 *  retrieved using the OpData variant, it will trigger an assert when
	 *  adding data to the UniString buffer that are not uni_char aligned.
	 *
	 *  @param dst Destination string to append data to
	 *  @param retrieve_limit If not 0, limits the number of characters retrieved
	 *  @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS RetrieveData(UniString &dst, size_t retrieve_limit = 0);

	/** Get the current position in the resource. The next call to RetrieveData
	 *  will return the data at this position. The previous call to
	 *  RetrieveData returned the data up to but not including this position.
	 *  @return The current absolute position (in bytes)
	 */
	OpFileLength GetPosition() { return m_data_descriptor->GetPosition(); }

	/** Rewind to a cached position in the resource. On success, you may call
	 *  RetrieveData immediately, which will return the data at this position.
	 *
	 *  It will also work to set a position greater than the current position,
	 *  provided that the given position is already cached (which is not
	 *  generally the case with resources in the process of being downloaded).
	 *
	 *  @param pos The absolute position to set (in bytes)
	 *  @return OK on success,
	 *          ERR_OUT_OF_RANGE if the position is not in the cached range.
	 */
	OP_STATUS SetPosition(OpFileLength pos);

	/** @return TRUE if we have reached the end of the resource (there will be no more data available in the future). */
	BOOL IsFinished() { return m_data_descriptor->FinishedLoading(); }

	/** @return the content type of the data, as detected from the response or overridden in OpResource::GetDescriptor. */
	URLContentType GetContentType() const { return m_data_descriptor->GetContentType(); }

	/** @return the character set of the data, as detected from the response or overridden in OpResource::GetDescriptor. */
	unsigned short GetCharsetID() const { return m_data_descriptor->GetCharsetID(); }

	/** If a text-type response does not have a header-specified charset, we
	 *  will start loading and decoding the resource based on a charset
	 *  guessed from the context, while at the same time running the data
	 *  through a charset-detector. If there is a mismatch between the
	 *  detected charset and the guessed charset when the resource is fully
	 *  loaded, this will trigger a new OnResponseAvailable callback with the
	 *  correct charset applied.
	 *
	 *  If the resource itself contains authoritative information about the
	 *  charset, for instance in the form of a <META> tag, this function can
	 *  be used to stop guessing the character set and avoid wasting cycles.
	 *  The check that the correct charset was used and possible reload of
	 *  the resource must then be executed externally.
	 */
	void StopGuessingCharset() { m_data_descriptor->StopGuessingCharset(); }

	/** @return the character offset of the first invalid character
	 *  (according to the charset decoder), or -1 if not known or if no
	 *  invalid characters have been found.
	 */
	OpFileLength GetFirstInvalidCharacterOffset() { return m_data_descriptor->GetFirstInvalidCharacterOffset(); }

private:
	OpDataDescriptor(URL_DataDescriptor *data_descriptor);
	friend class OpResourceImpl;

	URL_DataDescriptor *m_data_descriptor;
};

#endif
