/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OAUC_NETWORK_H
# define OAUC_NETWORK_H

# include "../common/common.h"

namespace opera_update_checker
{
  namespace network
  {
    /** The class hiding the details of the network layer using the HTTP protocol. */
    class Network
    {
    private:
      Network(Network&) {}
    protected:
      Network() {}
      virtual ~Network() {}
    public:
      /** Method which should be used when issuing the request. */
      enum Method
      {
        GET, /**< Use HTTP GET method. */
        POST /**< Use HTTP POST method. */
      };

      /** The struct encapsulating the request data. */
      struct RequestData
      {
        RequestData() : data(NULLPTR), data_mime_type(NULLPTR)
        {
        }
        /** The data. */
        const char* data;
        /** The length of the data. */
        unsigned data_length;
        /** The mime type of the data. May be NULL. It's ignored when the method is POST as application/x-www-form-urlencoded is assumed then. */
        const char* data_mime_type;
      };

    /** The location of the certificate to be used to verify https host. */
    enum CertificateLocation
    {
      /** The certificate is in a file. */
      CERTIFICATE_FILE,
      /** The certificate could be found in a directory. */
      CERTIFICATE_DIR,
      /** The certificate is in the memory. */
      CERTIFICATE_MEMORY
    };  // enum CertificateLocation

    /** Info about the certificate to be used. */
    struct CertificateInfo
    {
      /**
        * If location is CERTIFICATE_DIR or CERTIFICATE_FILE it's the path to
        * the certificate store/file. Otherwise it's certificate's data.
        * Must not be NULLPTR.
        */
      const char* certificate;
      /** Format of the certificate e.g. "PEM". Must not be NULLPTR. */
      const char* certificate_format;
      /** Specifies if the certificate is in the memory or in the filesystem. */
      CertificateLocation location;
    };  // struct CertificateInfo

      /** The response observer interface. Must be overridden by interested in getting the response. */
      class ResponseObserver
      {
      public:
        /** Called when some header is got. */
        virtual void OnHeaderReceived(const char* header, unsigned long data_len) = 0;
        /** Called when some data (other then an header) is got. */
        virtual void OnDataReceived(const char* data, unsigned long data_len) = 0;
      };

      /** The factory method. */
      static Network* Create();
      /** The terminator. */
      static void Destroy(Network* network);

      /** Sends the given request to a server given by the url using the specified method.
       * Blocks until the comunnication is done.
       *
       * @param url[in] - the server url to send the request to.
       * @param is_secure[in] - true if HTTPs protocol should be used.
       * @param cert[in] - relevant only if is_secure is true. Contains the certificate to be used for host verification. @see CertificateInfo.
       * @param method[in] - the method to be used. @see Method.
       * @param data[in] - the data to be sent. Some parts of it may be ignored depending on the method used. @see RequestData.
       * @param observer[in] - the observer to be notified about the response.
       *
       * @return StatusCode. @see StatusCode.
       */
      virtual status::Status SendHTTPRequest(const char* url, bool is_secure, const CertificateInfo* cert, Method method, const RequestData& data, ResponseObserver* observer) = 0;

      /** Gets the HTTP response code. The return value will be zero if no server response code has been received. */
      virtual unsigned GetResponseCode() = 0;
    };
  }
}
#endif // OAUC_NETWORK_H
