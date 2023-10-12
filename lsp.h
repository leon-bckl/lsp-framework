#pragma once

#include "json.h"

namespace jsonrpc{
	struct Message{
		json::String jsonrpc = "2.0";
	};

	enum class ErrorCode{
		ParseError                     = -32700,
		InvalidRequest                 = -32600,
		MethodNotFound                 = -32601,
		InvalidParams                  = -32602,
		InternalError                  = -32603,
		jsonrpcReservedErrorRangeStart = -32099,
		ServerNotInitialized           = -32002,
		UnknownErrorCode               = -32001,
		jsonrpcReservedErrorRangeEnd   = -32000,
		lspReservedErrorRangeStart     = -32899,
		RequestFailed                  = -32803,
		ServerCancelled                = -32802,
		ContentModified                = -32801,
		RequestCancelled               = -32800,
		lspReservedErrorRangeEnd       = -32800
	};
}
