#pragma once

#include "json.h"

namespace lsp::json{

class Parser{
public:
	Parser(std::string_view text);
	~Parser();

	[[nodiscard]] auto atEnd() const -> bool;
	[[nodiscard]] auto textOffset(const char* pos) const -> int;
	[[nodiscard]] auto currentTextOffset() const -> int;
	[[nodiscard]] auto parse() -> Value;
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
	auto currentState() const -> State;
	auto currentValue() -> Value&;
	void pushState(State state, Value& value);
	void popState();
	void skipWhitespace();
	auto parseString() -> String;
	auto parseNumber() -> Value;
	auto parseIdentifier() -> Value;
	auto parseSimpleValue() -> Value;
};


} // namespace lsp::json
