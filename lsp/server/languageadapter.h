#pragma once

namespace lsp{

struct Message;

namespace server{


class LanguageAdapter{
public:
	virtual ~LanguageAdapter() = default;

	virtual void processMessage(Message* message) = 0;
};

}
}
