/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

/*
 * SSL_varvector32 is the base class for the SSL_varvector8, 
 * SSL_varvector16 and SSL_varvector24 classes. The numbers 8, 16, 24 or 32 
 * denotes the number of bits used in the length of the vector, and also how 
 * many bits will be used in the length written in the vectors binary 
 * representation.
 * 
 * Baseclass
 *   SSL_Error_Status : Register last error to occur.
 *
 * data:
 *   length : length of the current vector
 *   data   : Pointer to vector at least length bytes;
 *            if length and alloclength are zero, data is NULL,
 *            
 *   
 *   alloclength : actual length of data vector, if length is zero
 *                 alloclength is zero
 *   maxlength   : maximum length which this vector can have,
 *                 set by the subclass
 *                               
 *   state  : loading state
 *   pos    : position in loading vector
 *   templen: used when loading length;
 *   
 * constructors:
 *   SSL_varvector32(uint32 mlen) : used only by subclasses, mlen is maxlength;
 *   SSL_varvector32()            : Creates 32 bit length vector 
 *   SSL_varvector32(const SSL_varvector32 &)  : Creates copy of a vector;
 * 
 * destructor : virtual, frees memory
 * 
 * virtual fuctions (protected)
 *   SizeOfLength() : returns  number of bytes in length, depends on subclass
 *                    8 bits gives 1, 16 gives 2 and so on.
 *   
 *   LoadLength(...): loads the length of the vector (SizeOfLength() bytes). same format as state load for integers.
 *                    returns pointer to next byte to be read from input, and updates left, status and pos.
 *   WriteLength(...): Writes length to output buffer, returns pointer to next position in buffer
 *                     writes SizeOfLength() bytes.
 *
 * virtual fuctions (public)
 *   operator =()   : copies vector, must be redefined by subclass
 *   Load()         : Loads maximum left bytes froms source into the vector, if object is idle, starts new loading
 *                    if there is not enough data in the source, further calls will load more data into the vector.
 *                    returns pointer to next byte to be read from input, and updates left.
 *   Write()        : Writes the vector, with the subclass specific length to the target buffer.
 *                    writes Size() bytes.
 *                    returns pointer to next position in buffer
 *   Input()  : Loads a number of bytes from the source into the vector starting at the given position
 *              returns pointer to next byte to be read from input, and updates left.
 *   Output() : Stores a number of bytes from the vectorr starting at the given position to the given buffer                      
 *              returns pointer to next position in buffer.
 *   Transfer(): Stores a number of bytes from the sourcevector starting at the given position
 *               into this vector starting at the specified position
 *               returns number of bytes written, pads if necessary
 *
 *   Size() : How many bytes the vector takes written to a buffer, including the byte specifying length;
 *   Resize() : Resizes the vector to the given length. Contents is only lost when reducing length
 *   GetLength() : The number of bytes in the vector, not counting the length bytes
 *   operator [] : get specified element byte
 *   GetDirect() : Returns pointer to the data. Be careful.
 *   
 *   operator == : compares the vector  true if they are the same.
 *   operator != : compares the vector  true if they are not the same. 
 *   
 *   FinishedLoading : True if the vwector is not loading data from a buffer
 *   Valid : True if all the invariants of the vector class are upheld. False if not finished loading. 
 *   
 *   operator << : Writes this vector to a stream
 *   operator >> : Reads this vector from a file, Loading must finish.
 *   
 */

#ifndef _SSLVAR_H_
#define _SSLVAR_H_

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_varvector32: public SSL_Error_Status, public DataStream_GenericRecord_Small
{
private:
    uint32 maxlength;
    uint32 alloc_block_size;

private:    
    void Init(uint32 mlen,uint32 lensize);
    
protected:
    SSL_varvector32(uint32 mlen,uint32 lensize); 

	void SetLegalMax(uint32 mlen); // Conditional, may not exceed maximum allowed vector size
    
public:
	
    SSL_varvector32(); 
    SSL_varvector32(const SSL_varvector32 &);
    virtual ~SSL_varvector32();
    void SetExternal(byte *);
	
    SSL_varvector32 &operator =(const SSL_varvector32 &);

	virtual OP_STATUS PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);
    void Resize(uint32);
    void SetResizeBlockSize(uint32 blocksize){alloc_block_size = (blocksize >16? blocksize : 16);};

    void FixedLoadLength(BOOL fix);//{fixedloadlength = fix;};
    void SetSecure(BOOL sec){/*securevector = sec;*/};
    
	uint32 GetLegalMaxLength() const{return maxlength;}; 

#ifdef SSL_FULL_VARVECTOR_SUPPORT
    byte &operator [](uint32);
    const byte &operator [](uint32) const;
#endif    
    
    byte *GetDirect(){return DataStream_GenericRecord_Small::GetDirectPayload();};
    const byte *GetDirect() const{return DataStream_GenericRecord_Small::GetDirectPayload();};
    BOOL operator ==(const SSL_varvector32 &) const;
    BOOL operator !=(const SSL_varvector32 &other) const {return !operator ==(other);};
	
    void Blank(byte val = 0x00);
    
	const byte *SetL(const byte *source,uint32 len);
    const byte *Set(const byte *,uint32 len);
    void Set(const char *);
    void Set(DataStream *);
    void Set(DataStream &src){Set(&src);}
	void ConcatL(SSL_varvector32 &first,SSL_varvector32 &second);
    void Concat(SSL_varvector32 &,SSL_varvector32 &);
    void Append(SSL_varvector32 &other);
    const byte *Append(const byte *, uint32 len);
    void Append(const char *source, uint32 len){Append((const byte *)source,len);};
    void Append(const char *);
    SSL_varvector32 &operator +=(const SSL_varvector32 &);
    SSL_varvector32 operator +(const SSL_varvector32 &); 
    
	void ForceVectorLengthSize(uint32 lensize, uint32 mlen);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_varvector32";}
#endif
};

class SSL_varvector8 : public SSL_varvector32
{
public:
    SSL_varvector8(): SSL_varvector32(0xff,1){}; 
    SSL_varvector8(const SSL_varvector32 &);
	
    SSL_varvector8 &operator =(const SSL_varvector32 &old){
		SSL_varvector32::operator =(old);
		return *this;
    };

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_varvector8";}
#endif
};

class SSL_varvector16 : public SSL_varvector32
{
public:
    SSL_varvector16(): SSL_varvector32(0xffff,2){}; 
    //SSL_varvector16(const SSL_varvector32 &old): SSL_varvector32(0xffff,2){operator =(old);}
	
    SSL_varvector16 &operator =(const SSL_varvector32 &old){
		SSL_varvector32::operator =(old);
		return *this;
    };

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_varvector16";}
#endif
};

class SSL_varvector24 : public SSL_varvector32
{
public:
    SSL_varvector24(): SSL_varvector32(0xffffff,3){}; 
    SSL_varvector24(const SSL_varvector32 &old): SSL_varvector32(0xffffff,3) {operator =(old);}
	
    SSL_varvector24 &operator =(const SSL_varvector32 &old){
		SSL_varvector32::operator =(old);
		return *this;
    };

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_varvector24";}
#endif
};

typedef SSL_varvector16 SSL_secure_varvector16;
typedef SSL_varvector32 SSL_secure_varvector32;

typedef SSL_varvector16 SSL_DistinguishedName;

typedef SSL_varvector24 SSL_ASN1Cert;

#endif
#endif
