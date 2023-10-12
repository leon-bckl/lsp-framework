#pragma once

namespace lsp{

enum class ErrorCode{
	ServerNotInitialized = -32002,
	UnknownErrorCode     = -32001,
	RequestFailed        = -32803,
	ServerCancelled      = -32802,
	ContentModified      = -32801,
	RequestCancelled     = -32800,
};

}
