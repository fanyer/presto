#ifndef  _SSL_SIGN_H_
#define _SSL_SIGN_H_

BOOL VerifySignedFile(URL &signed_file, const OpStringC8 &signature, const unsigned char *key, unsigned long key_len, SSL_HashAlgorithmType alg=SSL_SHA);
BOOL VerifyChecksum(URL &signed_file, const OpStringC8 &checksum, SSL_HashAlgorithmType alg);

/**
 * Check file signature (SHA 256 hash)
 */	
OP_STATUS CheckFileSignature(const OpString &full_path, const OpString8& signature);

#endif // _SSL_SIGN_H_
