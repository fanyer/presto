/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef NETWORK_IMPL_H
# define NETWORK_IMPL_H

# include "adjunct/autoupdate/autoupdate_checker/common/common.h"
# include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/network/curl/include/curl/curl.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/network.h"

class OAUCNetworkImpl : public opera_update_checker::network::Network
{
  class CURLManager
  {
    friend class OAUCNetworkImpl;
    CURL* easy_curl_;
    CURLSH* curl_share_;
    unsigned last_request_response_code_;
    static size_t WriteDataCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t WriteHeaderCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
#ifndef NO_OPENSSL
    static CURLcode SslCtxFunction(CURL* curl, void* sslctx, void* parm);
#endif // NO_OPENSSL
  public:
    CURLManager();
    ~CURLManager();
    template <typename option_type>
    void SetOption(CURLoption option, option_type value);
    opera_update_checker::status::Status SendRequest();
    opera_update_checker::status::Status PrepareNewRequest(ResponseObserver* observer);
    unsigned GetResponseCode() { return last_request_response_code_; }
  };
  CURLManager manager_;
  friend Network* Network::Create();
  static OAUCNetworkImpl self_;
public:
  virtual opera_update_checker::status::Status SendHTTPRequest(const char* url, bool is_secure, const CertificateInfo* cert, Method method, const RequestData& data, ResponseObserver* observer);
  virtual unsigned GetResponseCode() { return manager_.GetResponseCode(); }
};

#endif // NETWORK_IMPL_H
