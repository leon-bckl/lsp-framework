#pragma once

namespace lsp::server{

struct ClientCapapbilities;

class LanguageAdapter{
public:
	virtual ~LanguageAdapter() = default;

	virtual void initialize(const ClientCapapbilities& clientCapabilities) = 0;
};

}
