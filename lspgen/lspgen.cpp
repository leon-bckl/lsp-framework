#include <map>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <lsp/json/json.h>
#include <lsp/util/str.h>

/*
 * This is a huge mess because it started out as an experiment only.
 * But it works and should keep on working with upcoming lsp versions
 * unless there are fundamental changes to the meta model in which case
 * everything should be rewritten from scratch...
 */

using namespace lsp;

namespace strings{

#define STRING(x) const std::string x{#x};
STRING(documentation)
STRING(element)
STRING(extends)
STRING(items)
STRING(key)
STRING(kind)
STRING(method)
STRING(mixins)
STRING(name)
STRING(optional)
STRING(params)
STRING(partialResult)
STRING(properties)
STRING(registrationOptions)
STRING(result)
STRING(supportsCustomValues)
STRING(type)
STRING(value)
STRING(values)
#undef STRING

};

std::string extractDocumentation(const json::Object& json)
{
	if(json.contains("documentation"))
		return json.get<json::String>(strings::documentation);

	return {};
}

using TypePtr = std::unique_ptr<struct Type>;

struct Type{
	virtual ~Type() = default;

	enum Category{
		Base,
		Reference,
		Array,
		Map,
		And,
		Or,
		Tuple,
		StructureLiteral,
		StringLiteral,
		IntegerLiteral,
		BooleanLiteral
	};

	static constexpr std::string_view TypeCategoryStrings[] =
	{
		"base",
		"reference",
		"array",
		"map",
		"and",
		"or",
		"tuple",
		"literal",
		"stringLiteral",
		"integerLiteral",
		"booleanLiteral"
	};

	virtual Category category() const = 0;
	virtual void extract(const json::Object& json) = 0;

	static Category categoryFromString(std::string_view str)
	{
		for(std::size_t i = 0; i < std::size(TypeCategoryStrings); ++i)
		{
			if(TypeCategoryStrings[i] == str)
				return static_cast<Type::Category>(i);
		}

		throw std::runtime_error{'\'' + std::string{str} + "' is not a valid type kind"};
	}

	template<typename T>
	bool isA() const
	{
		return dynamic_cast<const T*>(this) != nullptr;
	}

	template<typename T>
	T& as()
	{
		return dynamic_cast<T&>(*this);
	}

	template<typename T>
	const T& as() const
	{
		return dynamic_cast<const T&>(*this);
	}

	static TypePtr createFromJson(const json::Object& json);
};

struct BaseType : Type{
	enum Kind{
		Boolean,
		String,
		Integer,
		UInteger,
		Decimal,
		URI,
		DocumentUri,
		RegExp,
		Null,
		MAX
	};

	static constexpr std::string_view BaseTypeStrings[] =
	{
		"boolean",
		"string",
		"integer",
		"uinteger",
		"decimal",
		"URI",
		"DocumentUri",
		"RegExp",
		"null"
	};

	Kind kind = {};

	Category category() const override{ return Category::Base; }

	void extract(const json::Object& json) override
	{
		kind = kindFromString(json.get<json::String>(strings::name));
	}

	static Kind kindFromString(std::string_view str)
	{
		for(std::size_t i = 0; i < std::size(BaseTypeStrings); ++i)
		{
			if(BaseTypeStrings[i] == str)
				return static_cast<Kind>(i);
		}

		throw std::runtime_error{'\'' + std::string{str} + "' is not a valid base type"};
	}
};

struct ReferenceType : Type{
	std::string name;

	Category category() const override{ return Category::Reference; }

	void extract(const json::Object& json) override
	{
		name = json.get<json::String>(strings::name);
	}
};

struct ArrayType : Type{
	TypePtr elementType;

	Category category() const override{ return Category::Array; }

	void extract(const json::Object& json) override
	{
		const auto& elementTypeJson = json.get<json::Object>(strings::element);
		elementType = createFromJson(elementTypeJson);
	}
};

struct MapType : Type{
	TypePtr keyType;
	TypePtr valueType;

	void extract(const json::Object& json) override
	{
		keyType = createFromJson(json.get<json::Object>(strings::key));
		valueType = createFromJson(json.get<json::Object>(strings::value));
	}

	Category category() const override{ return Category::Map; }
};

struct AndType : Type{
	std::vector<TypePtr> typeList;

	void extract(const json::Object& json) override
	{
		const auto& items = json.get<json::Array>(strings::items);
		typeList.reserve(items.size());

		for(const auto& i : items)
			typeList.push_back(createFromJson(i.get<json::Object>()));
	}

	Category category() const override{ return Category::And; }
};

struct OrType : Type{
	std::vector<TypePtr> typeList;

	void extract(const json::Object& json) override;

	Category category() const override{ return Category::Or; }
};

struct TupleType : Type{
	std::vector<TypePtr> typeList;

	void extract(const json::Object& json) override
	{
		const auto& items = json.get<json::Array>(strings::items);
		typeList.reserve(items.size());

		for(const auto& i : items)
			typeList.push_back(createFromJson(i.get<json::Object>()));
	}

	Category category() const override{ return Category::Tuple; }
};

struct StructureProperty{
	std::string name;
	TypePtr     type;
	bool        isOptional = false;
	std::string documentation;
};

struct StructureLiteralType : Type{
	std::vector<StructureProperty> properties;

	void extract(const json::Object& json) override
	{
		const auto& value = json.get<json::Object>(strings::value);
		const auto& propertiesJson = value.get<json::Array>(strings::properties);
		properties.reserve(propertiesJson.size());

		for(const auto& p : propertiesJson)
		{
			const auto& obj = p.get<json::Object>();
			auto& property = properties.emplace_back();

			property.name = obj.get<json::String>(strings::name);
			property.type = createFromJson(obj.get<json::Object>(strings::type));

			if(obj.contains(strings::optional))
				property.isOptional = obj.get<bool>(strings::optional);

			property.documentation = extractDocumentation(obj);
		}
	}

	Category category() const override{ return Category::StructureLiteral; }
};

void OrType::extract(const json::Object& json)
{
	const auto& items = json.get<json::Array>(strings::items);
	typeList.reserve(items.size());

	std::vector<std::unique_ptr<Type>> structureLiterals;

	for(const auto& item : items)
	{
		auto type = createFromJson(item.get<json::Object>());

		if(type->isA<StructureLiteralType>())
			structureLiterals.push_back(std::move(type));
		else
			typeList.push_back(std::move(type));
	}

	// Merge consecutive identical struct literals where the only difference is whether properties are optional or not

	for(std::size_t i = 1; i < structureLiterals.size(); ++i)
	{
		auto& first = structureLiterals[i - 1]->as<StructureLiteralType>();
		const auto& second = structureLiterals[i]->as<StructureLiteralType>();

		if(first.properties.size() == second.properties.size())
		{
			bool propertiesEqual = true;

			for(std::size_t p = 0; p < first.properties.size(); ++p)
			{
				if(first.properties[p].name != second.properties[p].name)
				{
					propertiesEqual = false;
					break;
				}
			}

			if(propertiesEqual)
			{
				for(std::size_t p = 0; p < first.properties.size(); ++p)
					first.properties[p].isOptional |= second.properties[p].isOptional;

				structureLiterals.erase(structureLiterals.begin() + static_cast<decltype(structureLiterals)::difference_type>(i));
				--i;
			}
		}
	}

	std::move(structureLiterals.begin(), structureLiterals.end(), std::back_inserter(typeList));
	structureLiterals.clear();

	if(typeList.empty())
		throw std::runtime_error{"OrType must not be empty!"};
}

struct StringLiteralType : Type{
	std::string stringValue;

	void extract(const json::Object& json) override
	{
		stringValue = static_cast<json::String>(json.get<json::String>(strings::value));
	}

	Category category() const override{ return Category::StringLiteral; }
};

struct IntegerLiteralType : Type{
	json::Integer integerValue = 0;

	void extract(const json::Object& json) override
	{
		integerValue = static_cast<json::Integer>(json.get(strings::value).numberValue());
	}

	Category category() const override{ return Category::IntegerLiteral; }
};

struct BooleanLiteralType : Type{
	bool booleanValue = false;

	void extract(const json::Object& json) override
	{
		booleanValue = json.get<json::Boolean>(strings::value);
	}

	Category category() const override{ return Category::BooleanLiteral; }
};

TypePtr Type::createFromJson(const json::Object& json)
{
	TypePtr result;
	auto category = categoryFromString(json.get<json::String>(strings::kind));

	switch(category)
	{
	case Base:
		result = std::make_unique<BaseType>();
		break;
	case Reference:
		result = std::make_unique<ReferenceType>();
		break;
	case Array:
		result = std::make_unique<ArrayType>();
		break;
	case Map:
		result = std::make_unique<MapType>();
		break;
	case And:
		result = std::make_unique<AndType>();
		break;
	case Or:
		result = std::make_unique<OrType>();
		break;
	case Tuple:
		result = std::make_unique<TupleType>();
		break;
	case StructureLiteral:
		result = std::make_unique<StructureLiteralType>();
		break;
	case StringLiteral:
		result = std::make_unique<StringLiteralType>();
		break;
	case IntegerLiteral:
		result = std::make_unique<IntegerLiteralType>();
		break;
	case BooleanLiteral:
		result = std::make_unique<BooleanLiteralType>();
		break;
	default:
		assert(!"Invalid type category");
		throw std::logic_error{"Invalid type category"};
	}

	assert(result->category() == category);
	result->extract(json);

	return result;
}

//================================

struct Enumeration{
	std::string name;
	TypePtr     type;

	struct Value{
		std::string name;
		json::Any   value;
		std::string documentation;
	};

	std::vector<Value> values;
	std::string        documentation;
	bool               supportsCustomValues = false;

	void extract(const json::Object& json)
	{
		name = json.get<json::String>(strings::name);
		const auto& typeJson = json.get<json::Object>(strings::type);
		type = Type::createFromJson(typeJson);
		const auto& valuesJson = json.get<json::Array>(strings::values);
		values.reserve(valuesJson.size());

		for(const auto& v : valuesJson)
		{
			const auto& obj = v.get<json::Object>();
			auto& enumValue = values.emplace_back();
			enumValue.name = obj.get<json::String>(strings::name);
			enumValue.value = obj.get(strings::value);
			enumValue.documentation = extractDocumentation(obj);
		}

		documentation = extractDocumentation(json);

		if(json.contains(strings::supportsCustomValues))
			supportsCustomValues = json.get<bool>(strings::supportsCustomValues);

		// Sort values so they can be more efficiently searched
		std::sort(values.begin(), values.end(), [this](const Value& lhs, const Value& rhs)
		{
			if(lhs.value.isString())
			{
				if(!rhs.value.isString())
					throw std::runtime_error{"Value type mismatch in enumeration '" + name + "'"};

				return lhs.value.get<json::String>() < rhs.value.get<json::String>();
			}
			else if(lhs.value.isNumber())
			{
				if(!rhs.value.isNumber())
					throw std::runtime_error{"Value type mismatch in enumeration '" + name + "'"};

				return lhs.value.numberValue() < rhs.value.numberValue();
			}

			throw std::runtime_error{"Invalid value type in enumeration" + name + "' - Must be string or number"};
		});
	}
};

struct Structure{
	std::string name;
	std::vector<StructureProperty> properties;
	std::vector<TypePtr>  extends;
	std::vector<TypePtr>  mixins;
	std::string           documentation;

	void extract(const json::Object& json)
	{
		name = json.get<json::String>(strings::name);
		const auto& propertiesJson = json.get<json::Array>(strings::properties);
		properties.reserve(propertiesJson.size());

		for(const auto& p : propertiesJson)
		{
			const auto& obj = p.get<json::Object>();
			auto& property = properties.emplace_back();
			property.name = obj.get<json::String>(strings::name);
			property.type = Type::createFromJson(obj.get<json::Object>(strings::type));

			if(obj.contains(strings::optional))
				property.isOptional = obj.get<json::Boolean>(strings::optional);

			property.documentation = extractDocumentation(obj);
		}

		if(json.contains(strings::extends))
		{
			const auto& extendsJson = json.get<json::Array>(strings::extends);
			extends.reserve(extendsJson.size());

			for(const auto& e : extendsJson)
				extends.push_back(Type::createFromJson(e.get<json::Object>()));
		}

		if(json.contains(strings::mixins))
		{
			const auto& mixinsJson = json.get<json::Array>(strings::mixins);
			mixins.reserve(mixinsJson.size());

			for(const auto& e : mixinsJson)
				mixins.push_back(Type::createFromJson(e.get<json::Object>()));
		}

		documentation = extractDocumentation(json);
	}
};

struct TypeAlias{
	std::string name;
	TypePtr     type;
	std::string documentation;

	void extract(const json::Object& json)
	{
		name = json.get<json::String>(strings::name);
		type = Type::createFromJson(json.get<json::Object>(strings::type));
		documentation = extractDocumentation(json);
	}
};

struct Message{
	enum class Direction{
		ClientToServer,
		ServerToClient,
		Both
	};

	std::string documentation;
	Direction   direction;
	// Those should be omitted if the strings are empty
	std::string paramsTypeName;
	std::string resultTypeName;
	std::string partialResultTypeName;
	std::string registrationOptionsTypeName;

	static std::string memberTypeName(const json::Object& json, const std::string& key)
	{
		if(!json.contains(key))
			return {};

		const auto& type = json.get<json::Object>(key);

		if(type.get<json::String>(strings::kind) == "reference")
			return type.get<json::String>(strings::name);

		return json.get<json::String>(strings::method) + util::str::capitalize(key);
	}

	void extract(const json::Object& json)
	{
		documentation = extractDocumentation(json);
		const auto& dir = json.get<json::String>("messageDirection");

		if(dir == "clientToServer")
			direction = Direction::ClientToServer;
		else if(dir == "serverToClient")
			direction = Direction::ServerToClient;
		else if(dir == "both")
			direction = Direction::Both;
		else
			throw std::runtime_error{"Invalid message direction: " + dir};

		paramsTypeName = memberTypeName(json, strings::params);
		resultTypeName = memberTypeName(json, strings::result);
		partialResultTypeName = memberTypeName(json, strings::partialResult);
		registrationOptionsTypeName = memberTypeName(json, strings::registrationOptions);
	}
};

class MetaModel{
public:
	MetaModel() = default;

	void extract(const json::Object& json)
	{
		extractMetaData(json);
		extractTypes(json);
		extractMessages(json);
	}

	std::variant<const Enumeration*, const Structure*, const TypeAlias*> typeForName(std::string_view name) const
	{
		if(auto it = m_typesByName.find(std::string{name}); it != m_typesByName.end())
		{
			switch(it->second.type)
			{
			case Type::Enumeration:
				return &m_enumerations[it->second.index];
			case Type::Structure:
				return &m_structures[it->second.index];
			case Type::TypeAlias:
				return &m_typeAliases[it->second.index];
			}
		}

		throw std::runtime_error{"Type with name '" + std::string{name} + "' does not exist"};
	}

	enum class MessageType{
		Request,
		Notification
	};

	const std::map<std::string, Message>& messagesByName(MessageType type) const
	{
		if(type == MessageType::Request)
			return m_requestsByMethod;

		assert(type == MessageType::Notification);
		return m_notificationsByMethod;
	}

	struct MetaData{
		std::string version;
	};

	const MetaData& metaData() const{ return m_metaData; }
	const std::vector<std::string_view>& typeNames() const{ return m_typeNames; }
	const std::vector<Enumeration>& enumerations() const{ return m_enumerations; }
	const std::vector<Structure>& structures() const{ return m_structures; }
	const std::vector<TypeAlias>& typeAliases() const{ return m_typeAliases; }

private:
	MetaData m_metaData;

	enum class Type{
		Enumeration,
		Structure,
		TypeAlias
	};

	struct TypeIndex{
		Type        type;
		std::size_t index;
	};

	std::vector<std::string_view>              m_typeNames;
	std::unordered_map<std::string, TypeIndex> m_typesByName;
	std::vector<Enumeration>                   m_enumerations;
	std::vector<Structure>                     m_structures;
	std::vector<TypeAlias>                     m_typeAliases;
	std::map<std::string, Message>             m_requestsByMethod;
	std::map<std::string, Message>             m_notificationsByMethod;

	void extractMetaData(const json::Object& json)
	{
		const auto& metaDataJson = json.get<json::Object>("metaData");
		m_metaData.version = metaDataJson.get<json::String>("version");
	}

	void extractTypes(const json::Object& json)
	{
		extractEnumerations(json);
		extractStructures(json);
		extractTypeAliases(json);
	}

	void extractMessages(const json::Object& json)
	{
		extractRequests(json);
		extractNotifications(json);
	}

	void extractRequests(const json::Object& json)
	{
		const auto& requests = json.get<json::Array>("requests");

		for(const auto& r : requests)
		{
			const auto& obj = r.get<json::Object>();
			const auto& method = obj.get<json::String>(strings::method);

			if(m_requestsByMethod.contains(method))
				throw std::runtime_error{"Duplicate request method: " + method};

			m_requestsByMethod[method].extract(obj);
		}
	}

	void extractNotifications(const json::Object& json)
	{
		const auto& notifications = json.get<json::Array>("notifications");

		for(const auto& r : notifications)
		{
			const auto& obj = r.get<json::Object>();
			const auto& method = obj.get<json::String>(strings::method);

			if(m_notificationsByMethod.contains(method))
				throw std::runtime_error{"Duplicate request method: " + method};

			m_notificationsByMethod[method].extract(obj);
		}
	}

	void insertType(const std::string& name, Type type, std::size_t index)
	{
		if(m_typesByName.contains(name))
			throw std::runtime_error{"Duplicate type '" + name + '\"'};

		auto it = m_typesByName.insert(std::pair(name, TypeIndex{type, index})).first;
		m_typeNames.push_back(it->first);
	}

	void extractEnumerations(const json::Object& json)
	{
		const auto& enumerations = json.get<json::Array>("enumerations");

		m_enumerations.resize(enumerations.size());

		for(std::size_t i = 0; i < enumerations.size(); ++i)
		{
			m_enumerations[i].extract(enumerations[i].get<json::Object>());
			insertType(m_enumerations[i].name, Type::Enumeration, i);
		}
	}

	void extractStructures(const json::Object& json)
	{
		const auto& structures = json.get<json::Array>("structures");

		m_structures.resize(structures.size());

		for(std::size_t i = 0; i < structures.size(); ++i)
		{
			m_structures[i].extract(structures[i].get<json::Object>());
			insertType(m_structures[i].name, Type::Structure, i);
		}
	}

	void addTypeAlias(const json::Object& json, const std::string& key, const std::string& typeBaseName)
	{
		if(json.contains(key))
		{
			const auto& typeJson = json.get<json::Object>(key);

			if(typeJson.get<json::String>(strings::kind) != "reference")
			{
				auto& alias = m_typeAliases.emplace_back();
				alias.name = typeBaseName + util::str::capitalize(key);
				alias.type = ::Type::createFromJson(typeJson);
				alias.documentation = extractDocumentation(typeJson);
				insertType(alias.name, Type::TypeAlias, m_typeAliases.size() - 1);
			}
		}
	}

	void extractTypeAliases(const json::Object& json)
	{
		const auto& typeAliases = json.get<json::Array>("typeAliases");

		m_typeAliases.resize(typeAliases.size());

		for(std::size_t i = 0; i < typeAliases.size(); ++i)
		{
			m_typeAliases[i].extract(typeAliases[i].get<json::Object>());
			insertType(m_typeAliases[i].name, Type::TypeAlias, i);
		}

		// Extract message and notification parameter and result types

		const auto& requests = json.get<json::Array>("requests");

		for(const auto& r : requests)
		{
			const auto& obj = r.get<json::Object>();
			const auto& typeBaseName = obj.get<json::String>(strings::method);

			addTypeAlias(obj, strings::result, typeBaseName);
			addTypeAlias(obj, strings::params, typeBaseName);
			addTypeAlias(obj, strings::partialResult, typeBaseName);
			addTypeAlias(obj, strings::registrationOptions, typeBaseName);
		}

		const auto& notifications = json.get<json::Array>("notifications");

		for(const auto& n : notifications)
		{
			const auto& obj = n.get<json::Object>();
			const auto& typeBaseName = obj.get<json::String>(strings::method);
			addTypeAlias(obj, strings::params, typeBaseName);
			addTypeAlias(obj, strings::registrationOptions, typeBaseName);
		}
	}
};

static constexpr const char* TypesHeaderBegin =
R"(#pragma once

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include <tuple>
#include <string>
#include <vector>
#include <variant>
#include <string_view>
#include <lsp/util/str.h>
#include <lsp/util/uri.h>
#include <lsp/json/json.h>
#include <lsp/serialization.h>
#include <lsp/util/nullable.h>

namespace lsp::types{

inline constexpr std::string_view VersionStr{"${LSP_VERSION}"};

using LSPArray  = json::Array;
using LSPObject = json::Object;
using LSPAny    = json::Any;

template<typename K, typename T>
using Map = util::str::UnorderedMap<K, T>;

)";

static constexpr const char* TypesHeaderEnd =
R"(} // namespace lsp::types
)";

static constexpr const char* TypesSourceBegin =
R"(#include "types.h"

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include <cassert>
#include <iterator>
#include <algorithm>

namespace lsp::types {

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter
#endif

template<typename E, typename D, typename T>
static E findEnumValue(const D* values, const T& value)
{
	auto begin = values;
	auto end = values + static_cast<int>(E::MAX_VALUE);
	auto it = std::lower_bound(begin, end, value);

	if(it == end || *it != value)
		return E::MAX_VALUE;

	return static_cast<E>(std::distance(begin, it));
}

)";

static constexpr const char* TypesSourceEnd =
R"(#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
} // namespace lsp::types
)";

static constexpr const char* MessagesHeaderBegin =
R"(#pragma once

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include "types.h"
#include <lsp/messagebase.h>

namespace lsp::messages{

)";

static constexpr const char* MessagesHeaderEnd =
R"(} // namespace lsp::messages
)";

static constexpr const char* MessagesSourceBegin =
R"(#include "messages.h"

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

namespace lsp::messages{

)";

static constexpr const char* MessagesSourceEnd =
R"(
std::string_view methodToString(Method method)
{
	return MethodStrings[static_cast<int>(method)];
}

Method methodFromString(std::string_view str)
{
	auto it = MethodsByString.find(str);

	if(it != MethodsByString.end())
		return it->second;

	return Method::INVALID;
}

} // namespace lsp::messages
)";

class CppGenerator
{
public:
	CppGenerator(const MetaModel* metaModel) : m_metaModel{*metaModel}{}

	void generate()
	{
		generateTypes();
		generateMessages();
	}

	void writeFiles()
	{
		writeFile("types.h", m_typesHeaderFileContent + m_typesBoilerPlateHeaderFileContent);
		writeFile("types.cpp", m_typesSourceFileContent + m_typesBoilerPlateSourceFileContent);
		writeFile("messages.h", m_messagesHeaderFileContent);
		writeFile("messages.cpp", m_messagesSourceFileContent);
	}

private:
	std::string                                  m_typesHeaderFileContent;
	std::string                                  m_typesBoilerPlateHeaderFileContent;
	std::string                                  m_typesBoilerPlateSourceFileContent;
	std::string                                  m_typesSourceFileContent;
	std::string                                  m_messagesHeaderFileContent;
	std::string                                  m_messagesSourceFileContent;
	const MetaModel&                             m_metaModel;
	std::unordered_set<std::string_view>         m_processedTypes;
	std::unordered_set<std::string_view>         m_typesBeingProcessed;
	std::unordered_map<const Type*, std::string> m_generatedTypeNames;

	struct CppBaseType
	{
		std::string data;
		std::string constData;
		std::string param;
		std::string getResult;
	};

	static const CppBaseType s_baseTypeMapping[BaseType::MAX];
	static const CppBaseType s_stringBaseType;

	void generateTypes()
	{
		m_processedTypes = {"LSPArray", "LSPObject", "LSPAny"};
		m_typesBeingProcessed = {};
		m_typesHeaderFileContent = util::str::replace(TypesHeaderBegin, "${LSP_VERSION}", m_metaModel.metaData().version);
		m_typesBoilerPlateHeaderFileContent = "\nnamespace lsp{\n\n/*\n * Serialization boilerplate\n */\n\n";
		m_typesBoilerPlateSourceFileContent = m_typesBoilerPlateHeaderFileContent;
		m_typesSourceFileContent = TypesSourceBegin;

		for(const auto& name : m_metaModel.typeNames())
			generateNamedType(name);

		m_typesHeaderFileContent += TypesHeaderEnd;
		m_typesSourceFileContent += TypesSourceEnd;
		m_typesBoilerPlateHeaderFileContent += "\n} // namespace lsp\n";
		m_typesBoilerPlateSourceFileContent += "} // namespace lsp\n";
	}

	void generateMessages()
	{
		// Message method enum

		m_messagesHeaderFileContent = std::string{MessagesHeaderBegin} + "enum class Method{\n"
		                              "\tINVALID = -1,\n\n"
		                              "\t// Requests\n\n";
		m_messagesSourceFileContent = std::string{MessagesSourceBegin} + "static constexpr std::string_view MethodStrings[static_cast<int>(Method::MAX_VALUE)] = {\n";

		std::string messageMethodsByString = "static auto methodStringPair(Method method)"
		                                     "{\n"
		                                     "\treturn std::make_pair(MethodStrings[static_cast<int>(method)], method);\n"
		                                     "}\n\n"
		                                     "static const util::str::UnorderedMap<std::string_view, Method> MethodsByString = {\n";
		std::string messageCreateFromMethod = "MessagePtr createFromMethod(messages::Method method)"
		                                      "{\n"
		                                      "\tswitch(method)\n\t{\n";

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Request))
		{
			auto id = upperCaseIdentifier(method);
			m_messagesHeaderFileContent += '\t' + id + ",\n";
			m_messagesSourceFileContent += '\t' + util::str::quote(method) + ",\n";
			messageMethodsByString += "\tmethodStringPair(Method::" + id + "),\n";
			messageCreateFromMethod += "\tcase messages::Method::" + id + ":\n";
			messageCreateFromMethod += "\t\treturn std::make_unique<messages::" + id + ">();\n";
		}

		m_messagesHeaderFileContent += "\n\t// Notifications\n\n";

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Notification))
		{
			auto id = upperCaseIdentifier(method);
			m_messagesHeaderFileContent += '\t' + id + ",\n";
			m_messagesSourceFileContent += '\t' + util::str::quote(method) + ",\n";
			messageMethodsByString += "\tmethodStringPair(Method::" + id + "),\n";
			messageCreateFromMethod += "\tcase messages::Method::" + id + ":\n";
			messageCreateFromMethod += "\t\treturn std::make_unique<messages::" + id + ">();\n";
		}

		messageCreateFromMethod += "\tdefault:\n"
		                           "\t\treturn nullptr;\n"
		                           "\t}\n"
		                           "}\n\n";

		m_messagesHeaderFileContent += "\tMAX_VALUE\n};\n\n";
		m_messagesSourceFileContent.pop_back();
		m_messagesSourceFileContent.pop_back();
		m_messagesSourceFileContent += "\n};\n\n";
		messageMethodsByString.pop_back();
		messageMethodsByString.pop_back();
		messageMethodsByString += "\n};\n\n";
		m_messagesSourceFileContent += messageMethodsByString + messageCreateFromMethod;

		// Structs

		m_messagesHeaderFileContent += documentationComment("Requests", {}) + '\n';

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Request))
			generateMessage(method, message, false);

		m_messagesHeaderFileContent += documentationComment("Notifications", {}) + '\n';

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Notification))
			generateMessage(method, message, true);

		m_messagesHeaderFileContent += MessagesHeaderEnd;
		m_messagesSourceFileContent += MessagesSourceEnd;
	}

	void generateMessage(const std::string& method, const Message& message, bool isNotification)
	{
		auto messageCppName = upperCaseIdentifier(method);

		m_messagesHeaderFileContent += documentationComment(method, message.documentation) +
		                               "struct " + messageCppName + " : ";

		switch(message.direction)
		{
		case Message::Direction::ClientToServer:
			m_messagesHeaderFileContent += "ClientToServer";
			break;
		case Message::Direction::ServerToClient:
			m_messagesHeaderFileContent += "ServerToClient";
			break;
		case Message::Direction::Both:
			m_messagesHeaderFileContent += "Bidirectional";
		}

		m_messagesHeaderFileContent += std::string{isNotification ? "Notification" : "Request"} + "{\n"
		                               "\tstatic constexpr auto MessageMethod = Method::" + messageCppName + ";\n";

		const bool hasRegistrationOptions = !message.registrationOptionsTypeName.empty();
		const bool hasPartialResult = !message.partialResultTypeName.empty();
		const bool hasParams = !message.paramsTypeName.empty();
		const bool hasResult = !message.resultTypeName.empty();

		if(hasRegistrationOptions || hasPartialResult || hasParams || hasResult)
			m_messagesHeaderFileContent += '\n';

		if(hasRegistrationOptions)
			m_messagesHeaderFileContent += "\tusing RegistrationOptions = types::" + upperCaseIdentifier(message.registrationOptionsTypeName) + ";\n";

		if(hasPartialResult)
			m_messagesHeaderFileContent += "\tusing PartialResult = types::" + upperCaseIdentifier(message.partialResultTypeName) + ";\n";

		if(hasParams)
			m_messagesHeaderFileContent += "\tusing Params = types::" + upperCaseIdentifier(message.paramsTypeName) + ";\n";

		if(hasResult)
			m_messagesHeaderFileContent += "\tusing Result = types::" + upperCaseIdentifier(message.resultTypeName) + ";\n";

		if(hasParams || hasResult || hasRegistrationOptions || hasPartialResult)
			m_messagesHeaderFileContent += '\n';

		if(hasParams)
			m_messagesHeaderFileContent += "\tParams params;\n";

		if(hasResult)
			m_messagesHeaderFileContent += "\tResult result;\n";

		m_messagesHeaderFileContent += "\n\tMethod method() const override;\n";
		m_messagesSourceFileContent += "Method " + messageCppName + "::method() const{ return MessageMethod; }\n";

		if(hasParams)
		{
			m_messagesHeaderFileContent += "\tvoid initParams(const json::Any& json) override;\n";
			m_messagesSourceFileContent += "void " + messageCppName + "::initParams(const json::Any& json){ fromJson(json, params); }\n";
		}

		if(hasResult)
		{
			m_messagesHeaderFileContent += "\tvoid initResult(const json::Any& json) override;\n";
			m_messagesSourceFileContent += "void " + messageCppName + "::initResult(const json::Any& json){ fromJson(json, result); }\n";
		}

		if(hasParams)
		{
			m_messagesHeaderFileContent += "\tjson::Any paramsJson() const override;\n";
			m_messagesSourceFileContent += "json::Any " + messageCppName + "::paramsJson() const{ return toJson(params); }\n";
		}

		if(hasResult)
		{
			m_messagesHeaderFileContent += "\tjson::Any resultJson() const override;\n";
			m_messagesSourceFileContent += "json::Any " + messageCppName + "::resultJson() const{ return toJson(result); }\n";
		}

		m_messagesHeaderFileContent += "};\n\n";
	}

	static void writeFile(const std::string& name, std::string_view content)
	{
		std::ofstream file{name, std::ios::trunc | std::ios::binary};
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	static std::string upperCaseIdentifier(std::string_view str)
	{
		if(str.starts_with('$'))
			str.remove_prefix(1);

		auto parts = util::str::splitView(str, "/", true);
		auto id = util::str::join(parts, "_", [](auto&& s){ return util::str::capitalize(s); });

		std::transform(id.cbegin(), id.cend(), id.begin(), [](char c)
		{
			if(!std::isalnum(c) && c != '_')
				return '_';

			return c;
		});

		return id;
	}

	static std::string lowerCaseIdentifier(std::string_view str)
	{
		return util::str::uncapitalize(str);
	}

	static std::string toJsonSig(const std::string& typeName)
	{
		return "template<>\njson::Any toJson(const types::" + typeName + "& value)";
	}

	static std::string fromJsonSig(const std::string& typeName)
	{
		return "template<>\nvoid fromJson(const json::Any& json, types::" + typeName + "& value)";
	}

	static std::string documentationComment(const std::string& title, const std::string& documentation, int indentLevel = 0)
	{
		std::string indent(indentLevel, '\t');
		std::string comment = indent + "/*\n";

		if(!title.empty())
			comment += indent + " * " + title + "\n";

		auto documentationLines = util::str::splitView(documentation, "\n");

		if(!documentationLines.empty())
		{
			if(!title.empty())
				comment += indent + " *\n";

			for(const auto& l : documentationLines)
				comment += util::str::trimRight(indent + " * " + util::str::replace(util::str::replace(l, "/*", "/_*"), "*/", "*_/")) + '\n';
		}
		else if(title.empty())
		{
		 return {};
		}

		comment += indent + " */\n";

		return comment;
	}

	void generateNamedType(std::string_view name)
	{
		if(m_processedTypes.contains(name))
			return;

		m_processedTypes.insert(name);
		std::visit([this](const auto* ptr)
		{
			m_typesBeingProcessed.insert(ptr->name);
			generate(*ptr);
			m_typesBeingProcessed.erase(ptr->name);
		}, m_metaModel.typeForName(name));
	}

	void generate(const Enumeration& enumeration)
	{
		if(!enumeration.type->isA<BaseType>())
			throw std::runtime_error{"Enumeration value type for '" + enumeration.name + "' must be a base type"};

		auto enumerationCppName = upperCaseIdentifier(enumeration.name);

		const auto& baseType = s_baseTypeMapping[enumeration.type->as<BaseType>().kind];
		std::string enumValuesVarName = enumerationCppName + "Values";

		m_typesHeaderFileContent += documentationComment(enumerationCppName, enumeration.documentation);
		std::string valueIndent;

		if(enumeration.supportsCustomValues)
		{
			m_typesHeaderFileContent += "class " + enumerationCppName + "{\n"
			                            "public:\n"
			                            "\tenum ValueIndex{\n";
			valueIndent = "\t\t";
		}
		else
		{
			m_typesHeaderFileContent += "enum class " + enumerationCppName + "{\n";
			valueIndent = "\t";
		}

		m_typesSourceFileContent += "static constexpr " + baseType.constData + ' ' +
		                            enumValuesVarName + "[static_cast<int>(" + enumerationCppName + "::MAX_VALUE)] = {\n";

		if(auto it = enumeration.values.begin(); it != enumeration.values.end())
		{
			m_typesHeaderFileContent += documentationComment({}, it->documentation, 1 + enumeration.supportsCustomValues) +
			                            valueIndent + util::str::capitalize(it->name);
			m_typesSourceFileContent += '\t' + json::stringify(it->value);
			++it;

			while(it != enumeration.values.end())
			{
				m_typesHeaderFileContent += ",\n" + documentationComment({}, it->documentation, 1 + enumeration.supportsCustomValues) + valueIndent + util::str::capitalize(it->name);
				m_typesSourceFileContent += ",\n\t" + json::stringify(it->value);
				++it;
			}
		}

		m_typesHeaderFileContent += ",\n" + valueIndent + "MAX_VALUE\n";

		if(enumeration.supportsCustomValues)
		{
			m_typesHeaderFileContent += "\t};\n\n"
			                            "\t" + enumerationCppName + "() = default;\n" +
			                            "\t" + enumerationCppName + "(ValueIndex index){ *this = index; }\n" +
			                            "\texplicit " + enumerationCppName + '(' + baseType.param + " value){ *this = value; }\n"
			                            "\t" + enumerationCppName + "& operator=(ValueIndex index);\n"
			                            "\t" + enumerationCppName + "& operator=(" + baseType.param + " newValue);\n"
																	"\tbool operator==(ValueIndex index) const{ return m_index == index; }\n"
																	"\tbool operator==(" + baseType.constData + " value) const{ return m_value == value; }\n"
			                            "\toperator const " + baseType.data + "&() const{ return m_value; }\n"
			                            "\tbool hasCustomValue() const{ return m_index == MAX_VALUE; }\n"
			                            "\t" + baseType.getResult + " value() const{ return m_value; }\n\n"
			                            "private:\n"
			                            "\tValueIndex m_index = MAX_VALUE;\n"
			                            "\t" + baseType.data + " m_value{};\n"
			                            "};\n\n";
		}
		else
		{
			m_typesHeaderFileContent += "};\n\n";
		}

		m_typesSourceFileContent += "\n};\n\n";

		auto toJson = toJsonSig(enumerationCppName);
		auto fromJson = fromJsonSig(enumerationCppName);

		m_typesBoilerPlateHeaderFileContent += toJson + ";\n" +
		                                       fromJson + ";\n";

		if(enumeration.supportsCustomValues)
		{
			m_typesSourceFileContent += enumerationCppName + "& " + enumerationCppName + "::operator=(ValueIndex index)\n"
			                            "{\n"
			                            "\tassert(index < MAX_VALUE);\n"
			                            "\tm_index = index;\n"
			                            "\tm_value = " + enumValuesVarName + "[index];\n"
			                            "\treturn *this;\n"
			                            "}\n\n" +
			                            enumerationCppName + "& " + enumerationCppName + "::operator=(" + baseType.param + " value)\n"
			                            "{\n"
			                            "\tm_index = findEnumValue<" + enumerationCppName + "::ValueIndex>(" + enumValuesVarName + ", value);\n"
			                            "\tm_value = value;\n"
			                            "\treturn *this;\n"
			                            "}\n\n";
			m_typesBoilerPlateSourceFileContent += toJson + "\n"
			                                       "{\n" +
																	           "\treturn toJson(value.value());\n}\n\n" +
																	           fromJson + "\n"
																	           "{\n"
																	           "\t" + baseType.data + " jsonVal;\n"
																	           "\tfromJson(json, jsonVal);\n"
			                                       "\tvalue = jsonVal;\n"
																	           "}\n\n";
		}
		else
		{
			m_typesBoilerPlateSourceFileContent += toJson + "\n"
			                                       "{\n"
																	           "\treturn toJson(types::" + enumValuesVarName + "[static_cast<int>(value)]);\n}\n\n" +
																	           fromJson + "\n"
																	           "{\n"
																	           "\t" + baseType.data + " jsonVal;\n"
																	           "\tfromJson(json, jsonVal);\n"
			                                       "\tvalue = types::findEnumValue<types::" + enumerationCppName + ">(types::" + enumValuesVarName + ", jsonVal);\n"
			                                       "\tif(value == types::" + enumerationCppName + "::MAX_VALUE)\n"
																	           "\t\tthrow json::TypeError{\"Invalid value for '" + enumerationCppName + "'\"};\n"
																	           "}\n\n";
		}
	}

	bool isStringType(const TypePtr& type)
	{
		if(type->isA<BaseType>())
		{
			const auto& base = type->as<BaseType>();

			return base.kind == BaseType::String || base.kind == BaseType::URI || base.kind == BaseType::DocumentUri || base.kind == BaseType::RegExp;
		}
		else if(type->isA<ReferenceType>())
		{
			const auto& ref = type->as<ReferenceType>();
			auto refType = m_metaModel.typeForName(ref.name);

			if(std::holds_alternative<const TypeAlias*>(refType))
				return isStringType(std::get<const TypeAlias*>(refType)->type);
		}

		return false;
	}

	std::string cppTypeName(const Type& type, bool optional = false)
	{
		std::string typeName;

		if(optional)
		{
			if(!type.isA<ReferenceType>() || !m_typesBeingProcessed.contains(type.as<ReferenceType>().name))
				typeName = "std::optional<";
			else
				typeName = "std::unique_ptr<";
		}

		switch(type.category())
		{
		case Type::Base:
			typeName += s_baseTypeMapping[static_cast<int>(type.as<BaseType>().kind)].data;
			break;
		case Type::Reference:
			typeName += upperCaseIdentifier(type.as<ReferenceType>().name);
			break;
		case Type::Array:
			{
				const auto& arrayType = type.as<ArrayType>();
				if(arrayType.elementType->isA<ReferenceType>() && arrayType.elementType->as<ReferenceType>().name == "LSPAny")
					typeName += "LSPArray";
				else
					typeName += "std::vector<" + cppTypeName(*arrayType.elementType) + '>';

				break;
			}
		case Type::Map:
			{
				const auto& keyType = type.as<MapType>().keyType;
				const auto& valueType = type.as<MapType>().valueType;

				if(isStringType(keyType))
					typeName += "Map<";
				else
					typeName += "std::unordered_map<";

				typeName += cppTypeName(*keyType) + ", " + cppTypeName(*valueType) + '>';

				break;
			}
		case Type::And:
			typeName += "LSPObject"; // TODO: Generate a proper and type should they ever actually be used by the LSP
			break;
		case Type::Or:
			{
				const auto& orType = type.as<OrType>();

				if(orType.typeList.size() > 1)
				{
					auto nullType = std::find_if(orType.typeList.begin(), orType.typeList.end(), [](const TypePtr& type)
					{
					                                                                               return type->isA<BaseType>() && type->as<BaseType>().kind == BaseType::Null;
					                                                                             });
					std::string cppOrType;

					if(nullType == orType.typeList.end())
						cppOrType = "std::variant<";
					else if(orType.typeList.size() > 2)
						cppOrType = "util::NullableVariant<";
					else
						cppOrType = "util::Nullable<";

					if(auto it = orType.typeList.begin(); it != orType.typeList.end())
					{
						if(it == nullType)
							++it;

						assert(it != orType.typeList.end());
						cppOrType += cppTypeName(**it);
						++it;

						while(it != orType.typeList.end())
						{
							if(it != nullType)
								cppOrType += ", " + cppTypeName(**it);

							++it;
						}
					}

					cppOrType += '>';

					typeName += cppOrType;
				}
				else
				{
					assert(!orType.typeList.empty());
					typeName += cppTypeName(*orType.typeList[0]);
				}

				break;
			}
		case Type::Tuple:
			{
				std::string cppTupleType = "std::tuple<";
				const auto& tupleType = type.as<TupleType>();

				if(auto it = tupleType.typeList.begin(); it != tupleType.typeList.end())
				{
					cppTupleType += cppTypeName(**it);
					++it;

					while(it != tupleType.typeList.end())
					{
						cppTupleType += ", " + cppTypeName(**it);
						++it;
					}
				}

				cppTupleType += '>';

				typeName += cppTupleType;
				break;
			}
		case Type::StructureLiteral:
			typeName += m_generatedTypeNames[&type];
			break;
		case Type::StringLiteral:
			typeName += "json::String";
			break;
		case Type::IntegerLiteral:
			typeName += "json::Integer";
			break;
		case Type::BooleanLiteral:
			typeName += "bool";
			break;
		default:
			assert(!"Invalid type category");
			throw std::logic_error{"Invalid type category"};
		}

		if(optional)
			typeName += '>';

		return typeName;
	}

	void generateAggregateTypeList(const std::vector<TypePtr>& typeList, const std::string& baseName)
	{
		// Only append unique number to type name if there are multiple structure literals
		auto structLiteralCount = std::count_if(typeList.begin(), typeList.end(), [](const TypePtr& type){ return type->isA<StructureLiteralType>(); });
		int unique = 0;
		for(const auto& t : typeList)
		{
			generateType(t, baseName + (structLiteralCount > 1 ? std::to_string(unique) : ""));
			unique += t->isA<StructureLiteralType>();
		}
	}

	void generateType(const TypePtr& type, const std::string& baseName, bool alias = false)
	{
		switch(type->category())
		{
		case Type::Reference:
			generateNamedType(type->as<ReferenceType>().name);
			break;
		case Type::Array:
			generateType(type->as<ArrayType>().elementType, baseName);
			break;
		case Type::Map:
			generateType(type->as<MapType>().keyType, baseName);
			generateType(type->as<MapType>().valueType, baseName);
			break;
		case Type::And:
			generateAggregateTypeList(type->as<AndType>().typeList, baseName + (alias ? "_Base" : ""));
			break;
		case Type::Or:
			generateAggregateTypeList(type->as<OrType>().typeList, baseName + (alias ? "_Variant" : ""));
			break;
		case Type::Tuple:
			generateAggregateTypeList(type->as<TupleType>().typeList, baseName + (alias ? "_Element" : ""));
			break;
		case Type::StructureLiteral:
			{
				// HACK:
				// Temporarily transfer ownership of property types to temporary structure so the same generator code can be used for structure literals.
				auto& structureLiteral = type->as<StructureLiteralType>();
				Structure structure;

				structure.name = baseName;
				m_generatedTypeNames[type.get()] = structure.name;
				structure.properties = std::move(structureLiteral.properties);

				generate(structure);

				// HACK: Transfer unique_ptr ownership back from the temporary structure
				structureLiteral.properties = std::move(structure.properties);

				break;
			}
		default:
			break;
		}
	}

	void generateStructureProperties(const std::vector<StructureProperty>& properties, std::string& toJson, std::string& fromJson, std::vector<std::string>& requiredProperties)
	{
		for(const auto& p : properties)
		{
			std::string typeName = cppTypeName(*p.type, p.isOptional);

			m_typesHeaderFileContent += documentationComment({}, p.documentation, 1) +
			                            '\t' + typeName + ' ' + p.name + ";\n";
			if(p.isOptional)
			{
				toJson += "\tif(value." + p.name + ")\n\t";
				fromJson += "\tif(auto it = json.find(\"" + p.name + "\"); it != json.end())\n\t"
				            "\tfromJson(it->second, value." + p.name + ");\n";
			}
			else
			{
				requiredProperties.push_back(p.name);
				fromJson += "\tfromJson(json.get(\"" + p.name + "\"), value." + p.name + ");\n";
			}

			toJson += "\tjson[\"" + p.name + "\"] = toJson(value." + p.name + ");\n";
		}
	}

	void generate(const Structure& structure)
	{
		std::string structureCppName = upperCaseIdentifier(structure.name);

		// Make sure dependencies are generated first

	{
			for(const auto& e : structure.extends)
				generateType(e, {});

			// Mixins technically don't have to be generated since their properties are directly copied into this structure.
			// However, generating them all here makes sure that all property types are also generated.
			for(const auto& m : structure.mixins)
				generateType(m, {});

			for(const auto& p : structure.properties)
				generateType(p.type, structureCppName + util::str::capitalize(p.name));
		}

		m_typesHeaderFileContent += documentationComment(structureCppName, structure.documentation) +
		                            "struct " + structureCppName;

		std::string propertiesToJson = "static void " + util::str::uncapitalize(structureCppName) + "ToJson("
		                               "const " + structureCppName + "& value, json::Object& json)\n{\n";
		std::string propertiesFromJson = "static void " + util::str::uncapitalize(structureCppName) + "FromJson("
		                                 "const json::Object& json, " + structureCppName + "& value)\n{\n";
		std::string requiredPropertiesSig = "template<>\nconst char** requiredProperties<types::" + structureCppName + ">()";
		std::vector<std::string> requiredPropertiesList;
		std::string requiredProperties = requiredPropertiesSig + "\n{\n\tstatic const char* properties[] = {\n";

		// Add base classes

		if(auto it = structure.extends.begin(); it != structure.extends.end())
		{
			const auto* extends = &(*it)->as<ReferenceType>();
			m_typesHeaderFileContent += " : " + extends->name;
			std::string lower = util::str::uncapitalize(extends->name);
			propertiesToJson += '\t' + lower + "ToJson(value, json);\n";
			propertiesFromJson += '\t' + lower + "FromJson(json, value);\n";
			++it;

			for(const auto& p : std::get<const Structure*>(m_metaModel.typeForName(extends->name))->properties)
			{
				if(!p.isOptional)
					requiredPropertiesList.push_back(p.name);
			}

			while(it != structure.extends.end())
			{
				extends = &(*it)->as<ReferenceType>();
				m_typesHeaderFileContent += ", " + extends->name;
				lower = util::str::uncapitalize(extends->name);
				propertiesToJson += '\t' + lower + "ToJson(value, json);\n";
				propertiesFromJson += '\t' + lower + "FromJson(json, value);\n";
				++it;

				for(const auto& p : std::get<const Structure*>(m_metaModel.typeForName(extends->name))->properties)
				{
					if(!p.isOptional)
						requiredPropertiesList.push_back(p.name);
				}
			}
		}

		m_typesHeaderFileContent += "{\n";

		// Generate properties

		for(const auto& m : structure.mixins)
		{
			if(!m->isA<ReferenceType>())
				throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a type reference"};

			auto type = m_metaModel.typeForName(m->as<ReferenceType>().name);

			if(!std::holds_alternative<const Structure*>(type))
				throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a structure type"};

			generateStructureProperties(std::get<const Structure*>(type)->properties, propertiesToJson, propertiesFromJson, requiredPropertiesList);
		}

		generateStructureProperties(structure.properties, propertiesToJson, propertiesFromJson, requiredPropertiesList);

		m_typesHeaderFileContent += "};\n\n";

		propertiesToJson += "}\n\n";
		propertiesFromJson += "}\n\n";

		for(const auto& p : requiredPropertiesList)
			requiredProperties += "\t\t\"" + p + "\",\n";

		requiredProperties += "\t\tnullptr\n\t};\n\treturn properties;\n}\n\n";

		std::string toJson = toJsonSig(structureCppName);
		std::string fromJson = fromJsonSig(structureCppName);

		if(!requiredPropertiesList.empty())
		{
			m_typesBoilerPlateHeaderFileContent += requiredPropertiesSig + ";\n";
			m_typesBoilerPlateSourceFileContent += requiredProperties;
		}

		m_typesBoilerPlateHeaderFileContent += toJson + ";\n" +
		                                       fromJson + ";\n";
		m_typesSourceFileContent += propertiesToJson + propertiesFromJson;
		m_typesBoilerPlateSourceFileContent += toJson + "\n"
		                                       "{\n"
		                                       "\tjson::Object obj;\n"
		                                       "\ttypes::" + util::str::uncapitalize(structureCppName) + "ToJson(value, obj);\n"
		                                       "\treturn obj;\n"
		                                       "}\n\n" +
		                                       fromJson + "\n"
		                                       "{\n"
		                                       "\tconst auto& obj = json.get<json::Object>();\n"
		                                       "\ttypes::" + util::str::uncapitalize(structureCppName) + "FromJson(obj, value);\n"
		                                       "}\n\n";
	}

	void generate(const TypeAlias& typeAlias)
	{
		auto typeAliasCppName = upperCaseIdentifier(typeAlias.name);

		generateType(typeAlias.type, typeAliasCppName, true);

		m_typesHeaderFileContent += documentationComment(typeAliasCppName, typeAlias.documentation) +
		                            "using " + typeAliasCppName + " = " + cppTypeName(*typeAlias.type) + ";\n\n";
	}
};

const CppGenerator::CppBaseType CppGenerator::s_baseTypeMapping[] =
{
	{"bool", "bool", "bool", "bool"},
	{"std::string", "std::string_view", "const std::string&", "const std::string&"},
	{"int", "int", "int", "int"},
	{"unsigned int", "unsigned int", "unsigned int", "unsigned int"},
	{"double", "double", "double", "double"},
	{"util::FileURI", "util::FileURI", "const util::FileURI&", "const util::FileURI&"},
	{"util::FileURI", "util::FileURI", "const util::FileURI&", "const util::FileURI&"},
	{"std::string", "std::string_view", "const std::string&", "const std::string&"},
	{"std::nullptr_t", "std::nullptr_t", "std::nullptr_t", "std::nullptr_t"}
};

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		std::cerr << "Expected the input file name as the first and only argument" << std::endl;
		return EXIT_FAILURE;
	}

	int ExitCode = EXIT_SUCCESS;
	const char* inputFileName = argv[1];

	if(std::ifstream in{inputFileName, std::ios::binary})
	{
		try
		{
			in.seekg(0, std::ios::end);
			std::streamsize size = in.tellg();
			in.seekg(0, std::ios::beg);
			std::string jsonText;
			jsonText.resize(static_cast<std::string::size_type>(size));
			in.read(&jsonText[0], size);
			in.close();
			auto json = json::parse(jsonText).get<json::Object>();
			MetaModel metaModel;
			metaModel.extract(json);
			CppGenerator generator{&metaModel};
			generator.generate();
			generator.writeFiles();
		}
		catch(const json::ParseError& e)
		{
			std::cerr << "JSON parse error at offset " << e.textPos() << ": " << e.what() << std::endl;
			ExitCode = EXIT_FAILURE;
		}
		catch(const std::exception& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
			ExitCode = EXIT_FAILURE;
		}
		catch(...)
		{
			std::cerr << "Unknown error" << std::endl;
			ExitCode = EXIT_FAILURE;
		}
	}
	else
	{
		std::cerr << "Failed to open " << inputFileName << std::endl;
		ExitCode = EXIT_FAILURE;
	}

	return ExitCode;
}
