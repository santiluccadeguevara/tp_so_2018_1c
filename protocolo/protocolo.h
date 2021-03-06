#ifndef PROTOCOL_PROTOCOL_H_
#define PROTOCOL_PROTOCOL_H_

typedef enum {

	PROTOCOL_ANY_CLOSE_CONNECTION,
	PROTOCOL_ANY_HANDSHAKE,
	PROTOCOL_ANY_SEND_MSG,
	PROTOCOL_PE_EXEC,
	PROTOCOL_EC_SET,
	PROTOCOL_CI_PERSIST_KEY,
	PROTOCOL_IC_PERSIST_KEY_SUCCESS,
	PROTOCOL_EC_GET,
	PROTOCOL_CP_IS_THIS_KEY_BLOCKED,
	PROTOCOL_EP_EXECUTION_SUCCESS,
	PROTOCOL_EP_I_AM_BLOCKED,
	PROTOCOL_PC_KEY_IS_BLOCKED,
	PROTOCOL_PC_KEY_IS_NOT_BLOCKED,
	PROTOCOL_CP_IS_KEY_BLOCKED_BY_EXECUTING_ESI,
    PROTOCOL_PC_KEY_BLOCKED_BY_EXECUTING_ESI,
	PROTOCOL_PC_KEY_NOT_BLOCKED_BY_EXECUTING_ESI,
	PROTOCOL_CP_BLOCK_KEY,
	PROTOCOL_PC_KEY_BLOCKED_SUCCESFULLY,
	PROTOCOL_CP_UNLOCK_KEY,
	PROTOCOL_PC_KEY_UNLOCKED_SUCCESFULLY,
	PROTOCOL_EP_FINISHED_SCRIPT,
	PROTOCOL_EP_DIDNT_FINISH_SCRIPT,
	PROTOCOL_EC_STORE,
	PROTOCOL_CE_EVERYTHING_OK,
	PROTOCOL_CE_YOU_ARE_BLOCKED,
	PROTOCOL_CE_ILLEGAL_OPERATION,
	PROTOCOL_EP_I_BROKE_THE_LAW,
	PROTOCOL_PC_KEY_STATUS,
	PROTOCOL_CP_KEY_STATUS

} protocol_id;

#endif
