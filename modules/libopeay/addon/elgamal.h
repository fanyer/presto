
#ifndef _ELGAMAL_H_
#define _ELGAMAL_H_

#ifdef NEED_LIBOPEAY_ELGAMAL

#ifdef __cplusplus
extern "C" {
#endif
/*int ElGamal_GenerateExchangeKey(DH *dh);*/
int ElGamal_encrypt(int flen, const unsigned char *from, unsigned char *to, BIGNUM **a, DH *dh);
int ElGamal_decrypt(int flen, const unsigned char *from, unsigned char *to, BIGNUM *a, DH *dh);
int ElGamal_sign(int type, const unsigned char *m, unsigned int m_len,
	     unsigned char *sigret, unsigned int *siglen, BIGNUM **a, DH *dh);
int ElGamal_verify(int dtype, const unsigned char *m, unsigned int m_len,
	     unsigned char *sigbuf, unsigned int siglen, BIGNUM *a, DH *dh);
#ifdef __cplusplus
}
#endif

#endif

#endif
