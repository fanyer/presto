/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "adjunct/autoupdate/autoupdate_checker/platforms/universal_adaptation_layer/networkimpl.h"
#ifndef NO_OPENSSL
#include <openssl/ssl.h>
#endif // NO_OPENSSL

namespace opera_update_checker { namespace network {

/* static */ Network* Network::Create()
{
  return &OAUCNetworkImpl::self_;
}

/* static */ void Network::Destroy(Network* net)
{
  // Nothing
}

} }

OAUCNetworkImpl OAUCNetworkImpl::self_;

namespace
{
  const long MAX_REDIRS = 16;
}

using namespace opera_update_checker::status;

OAUCNetworkImpl::CURLManager::CURLManager() : last_request_response_code_(0)
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl_share_ = curl_share_init();
  if (curl_share_)
  {
    curl_share_setopt(curl_share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(curl_share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(curl_share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  }
}

OAUCNetworkImpl::CURLManager::~CURLManager()
{
  if (easy_curl_)
    curl_easy_cleanup(easy_curl_);
  if (curl_share_)
     curl_share_cleanup(curl_share_);
  curl_global_cleanup();
}

/* static */ size_t OAUCNetworkImpl::CURLManager::WriteDataCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  ResponseObserver* obs = static_cast<ResponseObserver*>(userdata);
  if (obs)
    obs->OnDataReceived(ptr, size * nmemb);
  return size * nmemb;
}

/* static */ size_t OAUCNetworkImpl::CURLManager::WriteHeaderCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  ResponseObserver* obs = static_cast<ResponseObserver*>(userdata);
  if (obs)
    obs->OnHeaderReceived(ptr, size * nmemb);
  return size * nmemb;
}

template <typename option_type>
void OAUCNetworkImpl::CURLManager::SetOption(CURLoption option, option_type value)
{
  if (easy_curl_)
    curl_easy_setopt(easy_curl_, option, value);
}

Status OAUCNetworkImpl::CURLManager::PrepareNewRequest(ResponseObserver* observer)
{
  easy_curl_ = curl_easy_init();
  if (!easy_curl_)
    return StatusCode::FAILED;

  SetOption(CURLOPT_SHARE, curl_share_);
  SetOption(CURLOPT_HEADER, 0L);
  SetOption(CURLOPT_FOLLOWLOCATION, 1L);
  SetOption(CURLOPT_MAXREDIRS, MAX_REDIRS);
  SetOption(CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
  SetOption(CURLOPT_WRITEFUNCTION, WriteDataCallback);
  SetOption(CURLOPT_WRITEDATA, observer);
  SetOption(CURLOPT_HEADERFUNCTION, WriteHeaderCallback);
  SetOption(CURLOPT_HEADERDATA, observer);

  return StatusCode::OK;
}

Status OAUCNetworkImpl::CURLManager::SendRequest()
{
  if (easy_curl_)
  {
    CURLcode code = curl_easy_perform(easy_curl_);
    if (code == CURLE_OK)
      code = curl_easy_getinfo(easy_curl_, CURLINFO_RESPONSE_CODE, &last_request_response_code_);
    // Perform the clean up regardless any errors.
    curl_easy_cleanup(easy_curl_);
    easy_curl_ = NULLPTR;
    if (code != CURLE_OK)
      return code == CURLE_OUT_OF_MEMORY ? StatusCode::OOM : StatusCode::FAILED;

    return StatusCode::OK;
  }

  return StatusCode::FAILED;
}

#ifndef NO_OPENSSL
class SSLCtxCleaner {
  BIO* bio_;
  X509* x509_;
public:
  SSLCtxCleaner() : bio_(NULLPTR), x509_(NULLPTR) {
  }

  void SetBIO(BIO* bio) { bio_ = bio; }
  void SetX509(X509* x509) { x509_ = x509; }

  ~SSLCtxCleaner() {
    if (bio_)
      BIO_free(bio_);
    if (x509_)
      X509_free(x509_);
  }
};

CURLcode OAUCNetworkImpl::CURLManager::SslCtxFunction(CURL * curl, void * sslctx, void * parm)
{
  X509_STORE* store;
  X509* cert = NULLPTR;
  BIO* bio;
  bio = BIO_new_mem_buf(parm, -1);

  if (!bio)
    return CURLE_SSL_CERTPROBLEM;

  SSLCtxCleaner sslCtxCleaner;
  sslCtxCleaner.SetBIO(bio);
  PEM_read_bio_X509(bio, &cert, 0, NULLPTR);
  if (!cert)
    return CURLE_SSL_CERTPROBLEM;
  sslCtxCleaner.SetX509(cert);

  store = SSL_CTX_get_cert_store(static_cast<SSL_CTX *>(sslctx));

  if (!X509_STORE_add_cert(store, cert))
    return CURLE_SSL_CERTPROBLEM;

  return CURLE_OK;
}
#endif // !NO_OPENSSL

Status OAUCNetworkImpl::SendHTTPRequest(const char* url, bool https, const CertificateInfo* cert, Method method, const RequestData& data, ResponseObserver* observer)
{
#ifdef NO_OPENSSL
  if (cert && cert->location == CERTIFICATE_MEMORY)
    return StatusCode::NOT_SUPPORTED;
#endif // NO_OPENSSL

  if (!data.data)
    return StatusCode::INVALID_PARAM;

  Status status = manager_.PrepareNewRequest(observer);
  if (status != StatusCode::OK)
    return status;

  manager_.SetOption(CURLOPT_URL, url);
  if (method == POST)
  {
    manager_.SetOption(CURLOPT_POST, 1L);
    manager_.SetOption(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(data.data_length));
    manager_.SetOption(CURLOPT_COPYPOSTFIELDS, data.data);
  }

  if (https)
  {
    if (cert)
    {
      manager_.SetOption(CURLOPT_SSL_VERIFYPEER, 1L);
      manager_.SetOption(CURLOPT_SSLCERTTYPE, cert->certificate_format);
#ifndef NO_OPENSSL
      if (cert->location == CERTIFICATE_MEMORY)
      {
        manager_.SetOption(CURLOPT_SSL_CTX_DATA, cert->certificate);
        manager_.SetOption(CURLOPT_SSL_CTX_FUNCTION,  OAUCNetworkImpl::CURLManager::SslCtxFunction);
      }
      else
#endif // NO_OPENSSL
        manager_.SetOption(cert->location == CERTIFICATE_FILE ? CURLOPT_CAINFO : CURLOPT_CAPATH, cert->certificate);
    }
    else
      manager_.SetOption(CURLOPT_SSL_VERIFYPEER, 0L); // Don't verify the host. Make the connection lot less secure.
  }

  return manager_.SendRequest();
}
