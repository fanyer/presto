#ifndef CACHE_PROPAGATE_H

#define CACHE_PROPAGATE_H


// Always propagate the method (no return value)
#define CACHE_PROPAGATE_ALWAYS_VOID(METHOD_CALL)					\
{ if(next_manager) next_manager->METHOD_CALL; }

// Always propagate the method (return value)
//#define CACHE_PROPAGATE_ALWAYS_RETURN(METHOD_CALL, DEFAULT_RETURN)
//	{ return (next_manager) ? next_manager->METHOD_CALL : DEFAULT_RETURN; }

// Propagate the method when the next manager is not a remote manager (no return value)
#define CACHE_PROPAGATE_NO_REMOTE_VOID(METHOD_CALL)	\
	{ if (next_manager && !next_manager->IsRemoteManager()) next_manager->METHOD_CALL; }

// Propagate the method when the next manager is not a remote manager (return value)
#define CACHE_PROPAGATE_NO_REMOTE_RETURN(METHOD_CALL, DEFAULT_RETURN)	\
	{ return (next_manager && !next_manager->IsRemoteManager()) ? next_manager->METHOD_CALL : DEFAULT_RETURN; }

#endif // CACHE_PROPAGATE_H
