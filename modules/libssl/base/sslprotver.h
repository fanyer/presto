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

#ifndef __SSLPROTVER_H
#define __SSLPROTVER_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/datastream/fl_bytes.h"
#include "modules/libssl/base/sslenum.h"

#define SSL_Major_Protocol_Version buf[0]
#define SSL_Minor_Protocol_Version buf[1]


class SSL_ProtocolVersion : public DataStream_SourceBuffer
{
private:
    byte buf[2]; /* ARRAY OK 2009-07-08 yngve */
    
public:
    SSL_ProtocolVersion();
    SSL_ProtocolVersion(uint8 ma, uint8  mi);
    SSL_ProtocolVersion(const SSL_ProtocolVersion &old);

	/** Sets the protocol version based on the feature status, but limited by the protocol being activated by the preferences */
    OP_STATUS SetFromFeatureStatus(TLS_Feature_Test_Status protocol);

    void Set(uint8 ma, uint8 mi){SSL_Major_Protocol_Version = ma; SSL_Minor_Protocol_Version=mi;OP_ASSERT(SSL_Major_Protocol_Version != 0);};
    void Get(uint8 &ma, uint8 &mi) const {ma= SSL_Major_Protocol_Version; mi=SSL_Minor_Protocol_Version;};

    uint8 Major() const{return SSL_Major_Protocol_Version;};
    uint8 Minor() const{return SSL_Minor_Protocol_Version;};
    SSL_ProtocolVersion &operator =(const SSL_ProtocolVersion &other);

	operator uint16() const{ 
		return (SSL_Major_Protocol_Version < 3 ? (uint16) SSL_Major_Protocol_Version : 
				((((uint16) SSL_Major_Protocol_Version) << 8) | ((uint16) SSL_Minor_Protocol_Version)));}

	int Compare(uint8 major, uint8 minor) const;
#ifdef SSL_FULL_PROTOCOLVERSION_SUPPORT
    int operator ==(const SSL_ProtocolVersion &other) const {
        return (SSL_Major_Protocol_Version == other.SSL_Major_Protocol_Version) &&
            (SSL_Minor_Protocol_Version == other.SSL_Minor_Protocol_Version);
    };
#endif

    int operator !=(const SSL_ProtocolVersion &other) const {
        return (SSL_Major_Protocol_Version != other.SSL_Major_Protocol_Version) ||
            (SSL_Minor_Protocol_Version != other.SSL_Minor_Protocol_Version);
    };
    int operator <(const SSL_ProtocolVersion &other) const {
		return (SSL_Major_Protocol_Version == other.SSL_Major_Protocol_Version ? 
			(SSL_Minor_Protocol_Version < other.SSL_Minor_Protocol_Version) : SSL_Major_Protocol_Version < other.SSL_Major_Protocol_Version);
    };
#ifdef SSL_FULL_PROTOCOLVERSION_SUPPORT
    int operator >(const SSL_ProtocolVersion &other) const{
		return (SSL_Major_Protocol_Version == other.SSL_Major_Protocol_Version ?  
			(SSL_Minor_Protocol_Version > other.SSL_Minor_Protocol_Version) : SSL_Major_Protocol_Version > other.SSL_Major_Protocol_Version);
    };
    
    int operator <=(const SSL_ProtocolVersion &other) const{
		return (SSL_Major_Protocol_Version == other.SSL_Major_Protocol_Version ?  
			(SSL_Minor_Protocol_Version <= other.SSL_Minor_Protocol_Version) : SSL_Major_Protocol_Version <= other.SSL_Major_Protocol_Version);
    };
#endif
    int operator >=(const SSL_ProtocolVersion &other) const {
		return (SSL_Major_Protocol_Version == other.SSL_Major_Protocol_Version ? 
			(SSL_Minor_Protocol_Version >= other.SSL_Minor_Protocol_Version) : SSL_Major_Protocol_Version >= other.SSL_Major_Protocol_Version);
    };

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_ProtocolVersion";}
#endif
};

#endif
#endif
