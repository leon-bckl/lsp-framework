#include "json.h"

#include <vector>
#include <cassert>
#include <string>

namespace json{
namespace{

/*
 * Parser
 */

std::size_t textOffset(const char* start, const char* pos){
	return static_cast<std::size_t>(std::distance(start, pos));
}

class Parser{
public:
	Parser(std::string_view text) : m_start{text.data()},
	                                m_end{text.data() + text.size()},
	                                m_pos{m_start}{
		m_stateStack.reserve(10);
	}

	Value parse(){
		Value result;

		pushState(State::Value, result);

		while(!m_stateStack.empty()){
			skipWhitespace();

			if(m_pos >= m_end)
				throw ParseError{"Unexpected end of file", textOffset(m_start, m_pos)};

			switch(currentState()){
			case State::Value:
				handleValue();
				break;
			case State::Object:
				handleObject();
				break;
			case State::ObjectKey:
				handleObjectKey();
				break;
			case State::Array:
				handleArray();
				break;
			}
		}

		return result;
	}

	void reset(){
		m_stateStack.clear();
		m_pos = m_start;
	}

private:
	enum class State{
		Value,
		Object,
		ObjectKey,
		Array
	};

	struct StateStackEntry{
		State context;
		Value*    value;
	};

	std::vector<StateStackEntry> m_stateStack;
	const char* const            m_start = nullptr;
	const char* const            m_end   = nullptr;
	const char*                  m_pos   = nullptr;

	void handleValue(){
		assert(currentState() == State::Value);

		if(*m_pos == '{'){
			++m_pos;
			currentValue() = Object{};
			pushState(State::Object, currentValue());
		}else if(*m_pos == '['){
			++m_pos;
			currentValue() = Array{};
			pushState(State::Array, currentValue());
		}else{
			currentValue() = parseSimpleValue();
			popState();
		}
	}

	void handleObject(){
		assert(currentState() == State::Object);

		if(*m_pos == '}'){
			++m_pos;
			popState(); // Object
			popState(); // Value
		}else{
			if(!currentValueWithType<Object>().empty()){
				if(*m_pos != ',')
					throw ParseError{"Expected ','", textOffset(m_start, m_pos)};

				const char* pos = m_pos;
				++m_pos;
				skipWhitespace();

				if(m_pos < m_end && *m_pos == '}')
					throw ParseError{"Trailing ','", textOffset(m_start, pos)};
			}

			pushState(State::ObjectKey, currentValue());
		}
	}

	void handleObjectKey(){
		assert(currentState() == State::ObjectKey);

		const char* keyPos = m_pos;
		auto& object = currentValueWithType<Object>();
		auto key = parseString();

		if(object.contains(key))
			throw ParseError{"Duplicate key '" + key + "'", textOffset(m_start, keyPos)};

		skipWhitespace();

		if(m_pos < m_end && *m_pos != ':')
			throw ParseError{"Expected ':'", textOffset(m_start, m_pos)};

		++m_pos;

		popState();
		pushState(State::Value, object[key]);
	}

	void handleArray(){
		assert(currentState() == State::Array);

		if(*m_pos == ']'){
			++m_pos;
			popState(); // Array
			popState(); // Value
		}else{
			auto& array = currentValueWithType<Array>();

			if(!array.empty()){
				if(*m_pos != ',')
					throw ParseError{"Expected ','", textOffset(m_start, m_pos)};

				const char* pos = m_pos;
				++m_pos;
				skipWhitespace();

				if(m_pos < m_end && *m_pos == ']')
					throw ParseError{"Trailing ','", textOffset(m_start, pos)};
			}

			pushState(State::Value, array.emplace_back());
		}
	}

	State currentState() const{
		assert(!m_stateStack.empty());
		return m_stateStack.back().context;
	}

	Value& currentValue(){
		assert(!m_stateStack.empty());
		return *m_stateStack.back().value;
	}

	template<typename T>
	T& currentValueWithType(){
		assert(std::holds_alternative<T>(currentValue()));
		return std::get<T>(currentValue());
	}

	void pushState(State state, Value& value){
		m_stateStack.push_back({state, &value});
	}

	void popState(){
		assert(!m_stateStack.empty());
		m_stateStack.pop_back();
	}

	void skipWhitespace(){
		while(m_pos < m_end && std::isspace(*m_pos))
			++m_pos;
	}

	String parseString(){
		if(m_pos >= m_end || *m_pos != '\"')
			throw ParseError{"String expected", textOffset(m_start, m_pos)};

		const char* stringStart = ++m_pos;

		while(*m_pos != '\"' || m_pos[-1] == '\\'){
			++m_pos;

			if(m_pos >= m_end || *m_pos == '\n')
				throw ParseError{"Unmatched '\"'", textOffset(m_start, m_pos)};
		}

		const char* stringEnd = m_pos++;

		// TODO: Unescape string if necessary
		return {stringStart, stringEnd};
	}

	Number parseNumber(){
		const char* numberStart = m_pos;

		while(m_pos < m_end && (std::isalnum(*m_pos) || *m_pos == '-' || *m_pos == '.'))
			++m_pos;

		std::size_t idx = 0;
		Number number = std::stod(std::string{numberStart, m_pos}, &idx);

		if(idx < static_cast<std::size_t>(std::distance(numberStart, m_pos)))
			throw ParseError{"Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(m_start, numberStart)};

		return number;
	}

	Value parseIdentifier(){
		const char* idStart = m_pos;

		while(m_pos < m_end && std::isalnum(*m_pos))
			++m_pos;

		std::string_view id{ idStart, static_cast<std::size_t>(std::distance(idStart, m_pos)) };

		if(id == "true")
			return true;

		if(id == "false")
			return false;

		if(id == "null")
			return {};

		throw ParseError{"Unexpected '" + std::string{idStart, m_pos} + "'", textOffset(m_start, m_pos)};
	}

	Value parseSimpleValue(){
		if(*m_pos == '\"')
			return parseString();

		if(std::isdigit(*m_pos) || *m_pos == '-')
			return parseNumber();

		if(std::isalpha(*m_pos))
			return parseIdentifier();

		throw ParseError{"Unexpected token", textOffset(m_start, m_pos)};
	}
};

void stringifyImplementation(const Value& json, std::string& str){
	if(std::holds_alternative<Null>(json)){
		str += "null";
	}else if(std::holds_alternative<Boolean>(json)){
		str += std::get<Boolean>(json) ? "true" : "false";
	}else if(std::holds_alternative<Number>(json)){
		auto numberStr = std::to_string(std::get<Number>(json));

		while(!numberStr.empty()){
			if(numberStr.back() != '0')
				break;

			numberStr.pop_back();
		}

		if(numberStr.back() == '.')
			numberStr.pop_back();

		str += numberStr;
	}else if(std::holds_alternative<String>(json)){
		str += '\"' + std::get<String>(json) + '\"'; // TODO: Escape string if necessary
	}else if(std::holds_alternative<Object>(json)){
		const auto& obj = std::get<Object>(json);

		str += '{';

		auto it = obj.begin();

		if(it != obj.end()){
			str += '\"' + it->first + "\":" + stringify(it->second);
			++it;
		}

		while(it != obj.end()){
			str += ",\"" + it->first + "\":" + stringify(it->second);
			++it;
		}

		str += '}';
	}else if(std::holds_alternative<Array>(json)){
		const auto& array = std::get<Array>(json);

		str += '[';

		auto it = array.begin();

		if(it != array.end()){
			str += stringify(*it);
			++it;
		}

		while(it != array.end()){
			str += ',' + stringify(*it);
			++it;
		}

		str += ']';
	}
}

}

Value parse(std::string_view text){
	Parser parser{text};

	return parser.parse();
}

std::string stringify(const Value& json){
	std::string str;
	stringifyImplementation(json, str);
	return str;
}

}
