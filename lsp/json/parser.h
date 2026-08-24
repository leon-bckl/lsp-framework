#pragma once

#include "json.h"

namespace lsp::json{

class Parser{
public:
	Parser(std::string_view text);
	~Parser();

	bool atEnd() const;
	std::size_t textOffset(const char* pos) const;
	std::size_t currentTextOffset() const;
	Value parse();
	void reset();

private:
	enum class State;
	struct StateStackEntry;

	std::vector<StateStackEntry> m_stateStack;
	const char* const            m_start = nullptr;
	const char* const            m_end   = nullptr;
	const char*                  m_pos   = nullptr;

	void handleValue();
	void handleObject();
	void handleObjectKey();
	void handleArray();
	State currentState() const;
	Value& currentValue();
	void pushState(State state, Value& value);
	void popState();
	void skipWhitespace();
	String parseString();
	Value parseNumber();
	Value parseIdentifier();
	Value parseSimpleValue();
};


} // namespace lsp::json
