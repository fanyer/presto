/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNIX_PROCESS_TOKEN_H
#define UNIX_PROCESS_TOKEN_H

/** @brief Serializes arguments for new components in sub-processes
 */
class PosixIpcProcessToken
{
public:
	/** Constructor
	 * @param requester Address of component that requested the new component
	 * @param type Type of component to create in the sub-process
	 * @param cm_id ID of the component manager to create in the sub-process
	 * @param read_pipe Pipe to use for reading data from the main process
	 * @param write_pipe Pipe to use for writing data to the main process
	 */
	PosixIpcProcessToken(OpMessageAddress requester, OpComponentType type, int cm_id, int read_pipe, int write_pipe);

	/** Create a PosixIpcProcessToken from previously encoded data
	 * @param encoded Encoded process token
	 * @return New process token or NULL on failure. Caller owns return value
	 */
	static PosixIpcProcessToken* Decode(const char* encoded);

	/** Encode this process token into a string that can be passed around
	 * Token can be recreated using @ref Decode
	 */
	const char* Encode();

	/** @return Type of component to create in the sub-process
	 */
	OpComponentType GetComponentType() const { return m_type; }

	/** @return ID of the component manager to create in the sub-process
	 */
	int GetComponentManagerId() const { return m_cm_id; }

	/** @return Pipe to use for reading data from the main process
	 */
	int GetReadPipe() const { return m_read_fd; }

	/** @return Pipe to use for writing data to the main process
	 */
	int GetWritePipe() const { return m_write_fd; }

	/** @return Address of component that requested the new component
	 */
	OpMessageAddress GetRequester() const { return m_requester; }

private:
	OpData m_token;
	OpMessageAddress m_requester;
	OpComponentType m_type;
	int m_cm_id;
	int m_read_fd;
	int m_write_fd;
};

#endif // UNIX_PROCESS_TOKEN_H
