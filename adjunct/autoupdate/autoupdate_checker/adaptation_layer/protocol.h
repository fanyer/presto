/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef OAUC_XML_H
# define OAUC_XML_H

# include "../common/common.h"

namespace opera_update_checker
{
  namespace protocol
  {
    /** The class hiding the details of the request to the autoupdate server. */
    class Request
    {
    private:
      Request(Request&) {}
    protected:
      Request() {}
      virtual ~Request() {}
    public:
      /** The factory method returning an instance of this class. */
      static Request* Create();
      /** The terminator. */
      static void Destroy(Request* p);

      /** Provides the data needed to compose the main request part. The data is owned by the provider. */
      class UpdateDataProvider
      {
      public:
        virtual ~UpdateDataProvider() {}
        virtual const char* GetProductName() const = 0;
        virtual status::Status GetUUID(const char*&) const = 0;
        virtual status::Status GetLUT(const char*&) const = 0;
        virtual const char* GetOsName() const = 0;
        virtual const char* GetOsVersion() const = 0;
        virtual const char* GetArch() const = 0;
        virtual const char* GetPackage() const = 0;
        virtual const char* GetFirstRunVersion() const = 0;
        virtual const char* GetFirstRunTimestamp() const = 0;
        virtual const char* GetUserLocation() const = 0;
        virtual const char* GetDetectedLocation() const = 0;
        virtual const char* GetActiveLocation() const = 0;
        virtual const char* GetRegion() const = 0;
        virtual const char* GetProductLanguage() const = 0;
        virtual const char* GetProductVersion() const = 0;
      };

      /** Causes the main update check part (metrics, Opera binary) to be composed and added the the data.
       *
       * @param[in] provider - The update data provider.
       * @param[in] with_metrics - If true the metrics data will be included.
       *
       * @return StatusCode
       * @see StatusCode
       * @see UpdateDataProvider
       */
      virtual status::Status ComposeMainUpdatePart(const UpdateDataProvider& data_provider, bool with_metrics) = 0;
      /** Stores data about needed resources got from a product.
       *
       * @param[in] raw_data - the data to be added.
       *
       * @return StatusCode
       * @see StatusCode
       */
      virtual status::Status SetProductResourcesUpdatePart(const char* raw_data) = 0;
      /** Adds whole product data to the product specific data section.
       *
       * @param[in] provider - The update data provider.
       *
       * @return StatusCode
       * @see StatusCode
       * @see UpdateDataProvider
       */
      virtual status::Status AddProductUpdatePart(const UpdateDataProvider& provider) = 0;
      /** Gets the whole request data (might be NULL if no data has been composed/added).
       * The type and format of the data depends on the underlying implementation.
       */
      virtual const char* GetAllData() = 0;
      /** Clears the current content of the request. */
      virtual void Clear() = 0;
    };

    /** The class hiding the details of the request to the autoupdate server. */
    class Response
    {
    private:
      Response(Response&) {}
    protected:
      Response() {}
      virtual ~Response() {}
    public:
      /** The factory method returning an instance of this class. */
      static Response* Create();
      /** The terminator. */
      static void Destroy(Response* p);

      /** Adds the response data to be parsed when Parse() is called.
       *
       * @param[in] data - the response data.
       * @param[in] data_len - the length of the data.
       *
       * @return StatusCode
       * @see StatusCode
       */
      virtual status::Status AddData(const char* data, unsigned data_len) = 0;

      /** Parses the response data.
       *
       * @return StatusCode
       * @see StatusCode
       */
      virtual status::Status Parse() = 0;

      /** Gets UUID from the response. */
      virtual const char* GetUUID() = 0;

      /** Gets LUT (last update timestamp) from the response. */
      virtual const char* GetLUT() = 0;

      /** Gets the retry time. The retry time > 0 means the response is incomplete and the request
       * should be sent again after the given retry time. */
      virtual unsigned GetRetryTime() = 0;

      /** Gets the whole response data (might be NULL if no data has been composed/added).
       * The type and format of the data depends on the underlying implementation.
       */
      virtual const char* GetAllRawData() = 0;
      /** Clears the current content of the response. */
      virtual void Clear() = 0;
    };
  }
}

#endif // OAUC_XML_H