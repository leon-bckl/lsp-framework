#pragma once

namespace lsp{

struct ClientCapabilities;

namespace server{


class LanguageAdapter{
public:
	virtual ~LanguageAdapter() = default;

	virtual void initialize(const ClientCapabilities& clientCapabilities) = 0;
};

}
}
