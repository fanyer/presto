#ifndef ASYNCHKEYGENERATOR_H_
#define ASYNCHKEYGENERATOR_H_

#if defined _NATIVE_SSL_SUPPORT_
#include "modules/libopeay/addon/asynch_rsa_gen.h"
#include "modules/libssl/keygen_tracker.h"

class AsynchKeyPairGenerator : public SSL_Private_Key_Generator
{
private:
	RSA_KEYGEN_STATE_HANDLE *m_state_handle;
	int m_progress;

public:
	AsynchKeyPairGenerator();
	virtual ~AsynchKeyPairGenerator();

protected:

	/** Implementation specific
	 *
	 *  Set up the key generation process, 
	 *  If generation is finished, the implementation MUST call StoreKey
	 *
	 *	returns:
	 *
	 *		InstallerStatus::KEYGEN_FINISHED if the entire process is finished; a message will be posted, but need not be handled
	 *		OpStatus::OK  if the process continues, wait for finished message
	 */
	virtual OP_STATUS InitiateKeyGeneration();

	/** Implementation specific
	 * 
	 *	Execute the next step of the generation procedure
	 *  If generation is finished, the implementation MUST call StoreKey
	 *
	 *	returns:
	 *
	 *		OpStatus::OK  if the process continues, wait for finished message
	 *		InstallerStatus::ERR_PASSWORD_NEEDED if the security password is needed
	 */
	virtual OP_STATUS IterateKeyGeneration();

};

#endif
#endif /*ASYNCHKEYGENERATOR_H_*/
