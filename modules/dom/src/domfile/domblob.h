/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_BLOB_H
#define DOM_BLOB_H

#include "modules/dom/src/domobj.h"

class DOM_BlobReadView;
class DOM_FileReadView;
class DOM_FileReader_Base;

/**
 * The Blob interface in the W3C File API.
 */
class DOM_Blob
	: public DOM_Object
{
public:
	enum BlobType
	{
		BlobFile = 0,
		BlobString,
		BlobBuffer,
		BlobSlice,
		BlobSequence
	};

	/* The external API for creating (buffer) Blobs: */

	static OP_STATUS Make(DOM_Blob *&blob, ES_Object *array_buffer, BOOL is_array_view, DOM_Runtime *origining_runtime);
	/**< Create a Blob object based on an underlying ArrayBuffer or ArrayBufferView,
	     which can then be sliced and read, and converted using the FileReader
	     interface.

	     The ArrayBuffer isn't cloned but keeps a reference to the same
	     GCed object, hence any updates of the ArrayBuffer will be observable
	     when reading the Blob object.

	     @param [out] blob The result Blob.
	     @param array_buffer A previously created array object, must
	            reside in the same heap as origining_runtime's.
	     @param is_array_view TRUE if the array_buffer refers to a typed
	            array, FALSE if it is an array buffer.
	     @param origining_runtime The runtime to create the Blob in.
	     @return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS Make(DOM_Blob *&blob, unsigned char *data, unsigned length, DOM_Runtime *origining_runtime);
	/**< Create a Blob object from an external chunk of bytes. If the
	     object is successfully constructed, the Blob assumes ownership
	     of the external memory, which must have been allocated using
	     op_malloc().

	     @param [out] blob The result Blob.
	     @param buffer Externally allocated buffer, can be NULL if
	            empty.
	     @param length The length in bytes of 'buffer'.
	     @param origining_runtime The runtime to create the Blob in.
	     @return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual ~DOM_Blob()
	{
	}

	virtual OP_STATUS MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end) = 0;
	/**< Create a sliced view of this Blob, using the given start and end points.

	     @param [out]sliced_blob If successful, the result Blob.
	     @param slice_start The start byte offset. The argument is
	            assumed to be non-negative and finite.
	     @param slice_end The byte offset where the slice ends. The
	            argument is assumed to be non-negative and finite.
	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length) = 0;
	/**< Create a context object for reading a section of this Blob.

	     @param [out]blob_view If successful, the resulting read view object.
	     @param start_position The starting position.
	     @param view_length The number of bytes to read, including zero.
	            If FILE_LENGTH_NONE is passed, the Blob will be read to the end.
	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS Clone(DOM_Blob *source_object, ES_Runtime *target_runtime, DOM_Blob *&target_object);
	/**< The external Clone() method over Blobs. This method is intended used
	     by the structured cloning algorithm when it needs to clone the contents
	     of a Blob into a new context.

	     @param source_object The object to clone.
	     @param target_runtime The runtime to create the cloned object in.
	     @param [out]target_object If successful, the cloned result.
	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object);
	/**< The Blob cloning method. The implementation must be provided by each
	     kind of Blob supported.

	     @param target_runtime The runtime to create the cloned object in.
	     @param [out]target_object If successful, the cloned result.
	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef ES_PERSISTENT_SUPPORT
	/* see ES_CloneToPersistent documentation. */
	static OP_STATUS Clone(DOM_Blob *source_object, ES_PersistentItem *&target_item);

	/* see ES_CloneFromPersistent documentation. */
	static OP_STATUS Clone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object);

	virtual OP_STATUS MakeClone(ES_PersistentItem *&target_item);
#endif // ES_PERSISTENT_SUPPORT

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_BLOB || DOM_Object::IsA(type); }

	BlobType GetType() { return m_blob_type; }

	double GetSliceStart() { return m_sliced_start; }

	OpFileLength GetSliceLength() { return m_sliced_length == -1 ? FILE_LENGTH_NONE : static_cast<OpFileLength>(m_sliced_length); }

	const uni_char *GetForcedContentType() { return m_forced_content_type; }
	/**< Returns NULL if no content type has been forced. */

	BOOL IsSlice() const { return m_sliced_start != 0 || m_sliced_length != -1.0; }
	/**< @returns TRUE if a slice of a larger Blob. */

	virtual const uni_char *GetContentType();
	/**< @returns the Content Type if known, otherwise NULL. */

	virtual double GetSize() = 0;
	/**< @returns the size or 0 if not known. */

	DOM_DECLARE_FUNCTION(slice);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };

protected:
	DOM_Blob(BlobType type)
		: m_blob_type(type),
		  m_sliced_start(0),
		  m_sliced_length(-1.0)
	{
	}

	BlobType m_blob_type;
	/**< The kind of Blob this is. Used for internal type testing only. */

	OpString m_forced_content_type;

	/* A Blob can be sliced, meaning only a portion of it should be
	   returned. m_sliced_start and m_sliced_length indicates which
	   part of the underlying storage that this DOM_Blob provides. */

	double m_sliced_start;
	/**< The first byte to return in a slice. */

	double m_sliced_length;
	/**< The number of bytes in the slice (-1.0 means to the end.) */

	OP_STATUS ForceContentType(const uni_char *forced_content_type);
	/**< Set the user-requested content type for this Blob.

	     Returns OpStatus::ERR_NO_MEMORY if oom, OpStatus::OK if ok and
	     OpStatus::ERR if the content type isn't all-ASCII. */

	OP_STATUS ForceContentType(const char *forced_content_type);
	/**< Set the user-requested content type for this Blob.

	     @return OpStatus::ERR_NO_MEMORY if oom, otherwise OpStatus::OK. */

	friend class DOM_FileReader;
	friend class DOM_FileReaderSync;
};

/** Internal class that represents an ongoing read of a Blob. */
class DOM_BlobReadView
{
public:
	virtual ~DOM_BlobReadView()
	{
	}

	virtual BOOL AtEOF() = 0;
	/**< @returns TRUE when all bytes have been read from this view. */

	virtual OP_STATUS ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_buffer, unsigned prefix_length, OpFileLength *bytes_read) = 0;
	/**< Read data available from the Blob, prefixing the given characters to it.
	     The amount of data read is Blob dependent: if the data is available in
	     memory, all that data is read/copied over to the file reader in one go.

	     @param reader The reader object to add the available data to.
	     @param bytes  The pointer to the prefix data buffer, can be NULL.
	     @param length The length in characters of the prefix buffer.
	     @param [out]bytes_read On success, the number of bytes read from the
	            blob.
	     @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	              This method also propagates any errors caused when adding the
	              read data to the reader object,
	              @see DOM_FileReader::AppendResult(). */

	virtual void GCTrace() = 0;
};

/** DOM_BlobSlice is a sliced view of another Blob. */
class DOM_BlobSlice
	: public DOM_Blob
{
public:
	static OP_STATUS Make(DOM_BlobSlice *&blob, DOM_Blob *sliced_from, double slice_start, double slice_end, DOM_Runtime *origining_runtime);

	/* see base class documentation. */
	virtual OP_STATUS MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end);

	/* see base class documentation. */
	virtual OP_STATUS MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length);

	/* see base class documentation. */
	virtual double GetSize();

	/* see base class documentation. */
	virtual const uni_char *GetContentType();

	virtual void GCTrace();

	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object);
#ifdef ES_PERSISTENT_SUPPORT
	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_PersistentItem *&target_item);

	/* see base class documentation. */
	static OP_STATUS MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object);
#endif // ES_PERSISTENT_SUPPORT

	DOM_Blob *GetSlicedBlob() { return sliced_from; }

	BOOL IsSimpleSlice();
	/**< Returns TRUE if this is a slice of a BlobString or BlobBuffer.
	     Other kinds are considered non-simple when converting a BlobSlice
	     into a result. */

	const unsigned char *GetSliceBuffer();
	/**< Returns the start of the simple slice. Must only be called
	     on a slice where IsSimpleSlice() holds. */

protected:
	DOM_BlobSlice(DOM_Blob *sliced_from)
		: DOM_Blob(BlobSlice)
		, sliced_from(sliced_from)
	{
	}

	DOM_Blob *sliced_from;
};

/** DOM_BlobBuffer is an ArrayBuffer, ArrayBufferView or external memory
    wrapped up as a Blob. */
class DOM_BlobBuffer
	: public DOM_Blob
{
public:
	static OP_STATUS Make(DOM_BlobBuffer *&blob, ES_Object *array_buffer, BOOL is_array_view, DOM_Runtime *origining_runtime);

	static OP_STATUS Make(DOM_BlobBuffer *&blob, unsigned char *data, unsigned length, DOM_Runtime *origining_runtime);
	/**< Create a Blob object from an external chunk of bytes. If object
	     is successfully constructed, the Blob assumes ownership of the
	     external memory, which must have been allocated using op_malloc(). */

	/* see base class documentation. */
	virtual OP_STATUS MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end);

	/* see base class documentation. */
	virtual OP_STATUS MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length);

	/* see base class documentation.. */
	virtual double GetSize();

	OP_STATUS SetContentType(const uni_char *str);

	/* see base class documentation. */
	virtual const uni_char *GetContentType();

	virtual void GCTrace();

	const unsigned char *GetBuffer();

	ES_Object *GetArrayBufferView() { return array_buffer; }

	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object);
#ifdef ES_PERSISTENT_SUPPORT
	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_PersistentItem *&target_item);

	/* see base class documentation. */
	static OP_STATUS MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object);
#endif // ES_PERSISTENT_SUPPORT

protected:
	DOM_BlobBuffer(ES_Object *array_buffer, BOOL is_array_view)
		: DOM_Blob(BlobBuffer)
		, array_buffer(array_buffer)
		, is_array_view(is_array_view)
		, external_buffer(NULL)
		, external_length(0)
	{
	}

	DOM_BlobBuffer(unsigned char *buffer, unsigned length)
		: DOM_Blob(BlobBuffer)
		, array_buffer(NULL)
		, is_array_view(FALSE)
		, external_buffer(buffer)
		, external_length(length)
	{
	}

	virtual ~DOM_BlobBuffer();

	class ReadView
		: public DOM_BlobReadView
	{
	public:
		ReadView(DOM_BlobBuffer *blob_buffer, OpFileLength start, OpFileLength length)
			: blob_buffer(blob_buffer)
			, start_position(static_cast<unsigned>(start))
			, length_remaining(static_cast<unsigned>(length))
		{
		}

		virtual ~ReadView()
		{
		}

		virtual BOOL AtEOF() { return (length_remaining == 0); }

		/* see base class documentation. */
		virtual OP_STATUS ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read);

		virtual void GCTrace();

	private:
		friend class DOM_BlobBuffer;

		DOM_BlobBuffer *blob_buffer;
		unsigned start_position;
		unsigned length_remaining;
	};

	friend class DOM_BlobBuffer::ReadView;

	OpString content_type;

	ES_Object *array_buffer;
	BOOL is_array_view;
	/**< TRUE if 'array_buffer' refers to a typed array and its view of an underlying array buffer. */

	unsigned char *external_buffer;
	unsigned external_length;
};

/** DOM_BlobString are UTF-8 encoded strings in Blob form. */
class DOM_BlobString
	: public DOM_Blob
{
public:
	static OP_STATUS Make(DOM_BlobString *&blob, const uni_char *string, unsigned length, DOM_Runtime *origining_runtime);

	static OP_STATUS Make(DOM_BlobString *&blob, const char *string, unsigned length, DOM_Runtime *origining_runtime);

	/* see base class documentation. */
	virtual OP_STATUS MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end);

	/* see base class documentation. */
	virtual OP_STATUS MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length);

	/* see base class documentation. */
	virtual double GetSize();

	OP_STATUS SetContentType(const uni_char *str);

	/* see base class documentation. */
	virtual const uni_char *GetContentType();

	const unsigned char *GetString();

	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object);
#ifdef ES_PERSISTENT_SUPPORT
	/* see base class documentation. */
	static OP_STATUS MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object);

	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_PersistentItem *&target_item);
#endif // ES_PERSISTENT_SUPPORT

protected:
	DOM_BlobString()
		: DOM_Blob(BlobString)
		, string_length(0)
	{
	}

	OP_STATUS SetString(const uni_char *str, unsigned length);

	class ReadView
		: public DOM_BlobReadView
	{
	public:
		ReadView(DOM_BlobString *s, OpFileLength start, OpFileLength length)
			: blob_string(s)
			, start_position(static_cast<unsigned>(start))
			, length_remaining(static_cast<unsigned>(length))
		{
		}

		virtual ~ReadView()
		{
		}

		virtual BOOL AtEOF() { return (length_remaining == 0); }

		/* see base class documentation. */
		virtual OP_STATUS ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read);

		virtual void GCTrace();

	private:
		friend class DOM_BlobString;

		DOM_BlobString *blob_string;
		unsigned start_position;
		unsigned length_remaining;
	};

	friend class DOM_BlobString::ReadView;

	OpString8 string_value;
	/**< If non-empty, the UTF-8 sequence making up this blob. */

	unsigned string_length;
	/**< The UTF-8 encoded byte length of string_value. Kept
	     accurate in the presence of embedded NULs. */

	OpString content_type;
};

/** The Blob constructor creates a compound blob that can later be slice()d
    and read. Represented by a DOM_BlobSequence object, containing a sequence
    of String, ArrayBuffer and Blobs. */
class DOM_BlobSequence
	: public DOM_Blob
{
public:
	static OP_STATUS Make(DOM_BlobSequence *&blob_sequence, DOM_Runtime *origining_runtime);

	/* see base class documentation. */
	virtual OP_STATUS MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end);

	/* see base class documentation. */
	virtual OP_STATUS MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length);

	/* see base class documentation. */
	virtual double GetSize();

	virtual void GCTrace();

	OP_STATUS Append(DOM_Blob *blob);

	OP_STATUS SetContentType(const uni_char *t);

	/* see base class documentation. */
	virtual const uni_char *GetContentType();

	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object);
#ifdef ES_PERSISTENT_SUPPORT
	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_PersistentItem *&target_item);

	/* see base class documentation. */
	static OP_STATUS MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object);
#endif // ES_PERSISTENT_SUPPORT

protected:
	DOM_BlobSequence()
		: DOM_Blob(BlobSequence)
	{
	}

	OP_STATUS AppendSequence(DOM_BlobSequence *from);

	class ReadView
		: public DOM_BlobReadView
	{
	public:
		ReadView(DOM_BlobSequence *blob_sequence, OpFileLength start, OpFileLength length)
			: blob_sequence(blob_sequence)
			, start_position(start)
			, length_remaining(length)
			, current_blob_index(0)
			, current_blob_view(NULL)
		{
		}

		virtual ~ReadView();

		virtual BOOL AtEOF() { return (length_remaining == 0); }

		/* see base class documentation. */
		virtual OP_STATUS ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read);

		virtual void GCTrace();

	private:
		friend class DOM_BlobSequence;

		DOM_BlobSequence *blob_sequence;

		OpFileLength start_position;
		OpFileLength length_remaining;

		unsigned current_blob_index;

		DOM_BlobReadView *current_blob_view;
		/**< If non-NULL, the ongoing read view from a DOM_File. */
	};

	friend class DOM_BlobSequence::ReadView;

	OpVector<DOM_Blob> parts;
	OpString content_type;
};

class DOM_Blob_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_Blob_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::BLOB_PROTOTYPE)
	{
	}

	int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#ifdef ES_PERSISTENT_SUPPORT
class DOM_Blob_PersistentItem
	: public ES_PersistentItem
{
public:
	DOM_Blob_PersistentItem(DOM_Blob::BlobType blob_type)
		: blob_type(blob_type)
	{
	}

	virtual ~DOM_Blob_PersistentItem()
	{
	}

	virtual BOOL IsA(int tag) { return tag == DOM_TYPE_BLOB; }

	DOM_Blob::BlobType blob_type;
};
#endif // ES_PERSISTENT_SUPPORT

#endif // DOM_BLOB_H
