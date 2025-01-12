#include <map>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <lsp/str.h>
#include <lsp/json/json.h>

/*
 * This is a huge mess because it started out as an experiment only.
 * But it works and should keep on working with upcoming lsp versions
 * unless there are fundamental changes to the meta model in which case
 * everything should be rewritten from scratch...
 */

using namespace lsp;

namespace strings{

#define STRING(x) const std::string x{#x};
STRING(both)
STRING(clientToServer)
STRING(documentation)
STRING(element)
STRING(extends)
STRING(items)
STRING(key)
STRING(kind)
STRING(messageDirection)
STRING(method)
STRING(mixins)
STRING(name)
STRING(optional)
STRING(params)
STRING(partialResult)
STRING(errorData)
STRING(properties)
STRING(registrationOptions)
STRING(result)
STRING(serverToClient)
STRING(supportsCustomValues)
STRING(type)
STRING(value)
STRING(values)
#undef STRING

};

std::string extractDocumentation(const json::Object& json)
{
	if(json.contains(strings::documentation))
		return json.get(strings::documentation).string();

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

	bool isLiteral() const{
		const auto cat = category();
		return cat == StructureLiteral || cat == StringLiteral || cat == IntegerLiteral || cat == BooleanLiteral;
	}

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
		kind = kindFromString(json.get(strings::name).string());
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
		name = json.get(strings::name).string();
	}
};

struct ArrayType : Type{
	TypePtr elementType;

	Category category() const override{ return Category::Array; }

	void extract(const json::Object& json) override
	{
		const auto& elementTypeJson = json.get(strings::element).object();
		elementType = createFromJson(elementTypeJson);
	}
};

struct MapType : Type{
	TypePtr keyType;
	TypePtr valueType;

	void extract(const json::Object& json) override
	{
		keyType = createFromJson(json.get(strings::key).object());
		valueType = createFromJson(json.get(strings::value).object());
	}

	Category category() const override{ return Category::Map; }
};

struct AndType : Type{
	std::vector<TypePtr> typeList;

	void extract(const json::Object& json) override
	{
		const auto& items = json.get(strings::items).array();
		typeList.reserve(items.size());

		for(const auto& i : items)
			typeList.push_back(createFromJson(i.object()));
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
		const auto& items = json.get(strings::items).array();
		typeList.reserve(items.size());

		for(const auto& i : items)
			typeList.push_back(createFromJson(i.object()));
	}

	Category category() const override{ return Category::Tuple; }
};

struct StructureProperty{
	std::string name;
	TypePtr     type;
	bool        isOptional = false;
	std::string documentation;

	void extract(const json::Object& json)
	{
		name = json.get(strings::name).string();
		type = Type::createFromJson(json.get(strings::type).object());
		isOptional = json.contains(strings::optional) && json.get(strings::optional).boolean();
		documentation = extractDocumentation(json);
	}
};

using StructurePropertyList = std::vector<StructureProperty>;

StructurePropertyList extractStructureProperties(const json::Array& json)
{
	StructurePropertyList result;
	result.reserve(json.size());
	std::transform(
		json.begin(),
		json.end(),
		std::back_inserter(result),
		[](const json::Any& e)
		{
			StructureProperty prop;
			prop.extract(e.object());
			return prop;
		});
	// Sort properties so non-optional ones come first
	std::stable_sort(
		result.begin(),
		result.end(),
		[](const auto& p1, const auto& p2)
		{
			return !p1.isOptional && p2.isOptional;
		});

	return result;
}

struct StructureLiteralType : Type{
	StructurePropertyList properties;

	void extract(const json::Object& json) override
	{
		const auto& value = json.get(strings::value).object();
		properties = extractStructureProperties(value.get(strings::properties).array());
	}

	Category category() const override{ return Category::StructureLiteral; }
};

void OrType::extract(const json::Object& json)
{
	const auto& items = json.get(strings::items).array();
	typeList.reserve(items.size());

	std::vector<std::unique_ptr<Type>> structureLiterals;

	for(const auto& item : items)
	{
		auto type = createFromJson(item.object());

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
		stringValue = json.get(strings::value).string();
	}

	Category category() const override{ return Category::StringLiteral; }
};

struct IntegerLiteralType : Type{
	json::Integer integerValue = 0;

	void extract(const json::Object& json) override
	{
		integerValue = static_cast<json::Integer>(json.get(strings::value).number());
	}

	Category category() const override{ return Category::IntegerLiteral; }
};

struct BooleanLiteralType : Type{
	bool booleanValue = false;

	void extract(const json::Object& json) override
	{
		booleanValue = json.get(strings::value).boolean();
	}

	Category category() const override{ return Category::BooleanLiteral; }
};

TypePtr Type::createFromJson(const json::Object& json)
{
	TypePtr result;
	auto category = categoryFromString(json.get(strings::kind).string());

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
		name = json.get(strings::name).string();
		const auto& typeJson = json.get(strings::type).object();
		type = Type::createFromJson(typeJson);
		const auto& valuesJson = json.get(strings::values).array();
		values.reserve(valuesJson.size());

		for(const auto& v : valuesJson)
		{
			const auto& obj = v.object();
			auto& enumValue = values.emplace_back();
			enumValue.name = obj.get(strings::name).string();
			enumValue.value = obj.get(strings::value);
			enumValue.documentation = extractDocumentation(obj);
		}

		documentation = extractDocumentation(json);

		if(json.contains(strings::supportsCustomValues))
			supportsCustomValues = json.get(strings::supportsCustomValues).boolean();

		// Sort values so they can be more efficiently searched
		std::sort(values.begin(), values.end(), [this](const Value& lhs, const Value& rhs)
		{
			if(lhs.value.isString())
			{
				if(!rhs.value.isString())
					throw std::runtime_error{"Value type mismatch in enumeration '" + name + "'"};

				return lhs.value.string() < rhs.value.string();
			}
			else if(lhs.value.isNumber())
			{
				if(!rhs.value.isNumber())
					throw std::runtime_error{"Value type mismatch in enumeration '" + name + "'"};

				return lhs.value.number() < rhs.value.number();
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
		name = json.get(strings::name).string();
		properties = extractStructureProperties(json.get(strings::properties).array());

		if(json.contains(strings::extends))
		{
			const auto& extendsJson = json.get(strings::extends).array();
			extends.reserve(extendsJson.size());

			for(const auto& e : extendsJson)
				extends.push_back(Type::createFromJson(e.object()));
		}

		if(json.contains(strings::mixins))
		{
			const auto& mixinsJson = json.get(strings::mixins).array();
			mixins.reserve(mixinsJson.size());

			for(const auto& e : mixinsJson)
				mixins.push_back(Type::createFromJson(e.object()));
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
		name = json.get(strings::name).string();
		type = Type::createFromJson(json.get(strings::type).object());
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
	std::string errorDataTypeName;
	std::string registrationOptionsTypeName;

	static std::string memberTypeName(const json::Object& json, const std::string& key)
	{
		if(!json.contains(key))
			return {};

		const auto& type = json.get(key).object();

		if(type.get(strings::kind).string() == "reference")
			return type.get(strings::name).string();

		return json.get(strings::method).string() + str::capitalize(key);
	}

	void extract(const json::Object& json)
	{
		documentation = extractDocumentation(json);
		const auto& dir = json.get(strings::messageDirection).string();

		if(dir == strings::clientToServer)
			direction = Direction::ClientToServer;
		else if(dir == strings::serverToClient)
			direction = Direction::ServerToClient;
		else if(dir == strings::both)
			direction = Direction::Both;
		else
			throw std::runtime_error{"Invalid message direction: " + dir};

		paramsTypeName = memberTypeName(json, strings::params);
		resultTypeName = memberTypeName(json, strings::result);
		partialResultTypeName = memberTypeName(json, strings::partialResult);
		errorDataTypeName = memberTypeName(json, strings::errorData);
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
		const auto& metaDataJson = json.get("metaData").object();
		m_metaData.version = metaDataJson.get("version").string();
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
		const auto& requests = json.get("requests").array();

		for(const auto& r : requests)
		{
			const auto& obj = r.object();
			const auto& method = obj.get(strings::method).string();

			if(m_requestsByMethod.contains(method))
				throw std::runtime_error{"Duplicate request method: " + method};

			m_requestsByMethod[method].extract(obj);
		}
	}

	void extractNotifications(const json::Object& json)
	{
		const auto& notifications = json.get("notifications").array();

		for(const auto& r : notifications)
		{
			const auto& obj = r.object();
			const auto& method = obj.get(strings::method).string();

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
		const auto& enumerations = json.get("enumerations").array();

		m_enumerations.resize(enumerations.size());

		for(std::size_t i = 0; i < enumerations.size(); ++i)
		{
			m_enumerations[i].extract(enumerations[i].object());
			insertType(m_enumerations[i].name, Type::Enumeration, i);
		}
	}

	void extractStructures(const json::Object& json)
	{
		const auto& structures = json.get("structures").array();

		m_structures.resize(structures.size());

		for(std::size_t i = 0; i < structures.size(); ++i)
		{
			m_structures[i].extract(structures[i].object());
			insertType(m_structures[i].name, Type::Structure, i);
		}
	}

	void addTypeAlias(const json::Object& json, const std::string& key, const std::string& typeBaseName)
	{
		if(json.contains(key))
		{
			const auto& typeJson = json.get(key).object();

			if(typeJson.get(strings::kind).string() != "reference")
			{
				auto& alias = m_typeAliases.emplace_back();
				alias.name = typeBaseName + str::capitalize(key);
				alias.type = ::Type::createFromJson(typeJson);
				alias.documentation = extractDocumentation(typeJson);
				insertType(alias.name, Type::TypeAlias, m_typeAliases.size() - 1);
			}
		}
	}

	void extractTypeAliases(const json::Object& json)
	{
		const auto& typeAliases = json.get("typeAliases").array();

		m_typeAliases.resize(typeAliases.size());

		for(std::size_t i = 0; i < typeAliases.size(); ++i)
		{
			m_typeAliases[i].extract(typeAliases[i].object());
			insertType(m_typeAliases[i].name, Type::TypeAlias, i);
		}

		// Extract message and notification parameter and result types

		const auto& requests = json.get("requests").array();

		for(const auto& r : requests)
		{
			const auto& obj = r.object();
			const auto& typeBaseName = obj.get(strings::method).string();

			addTypeAlias(obj, strings::result, typeBaseName);
			addTypeAlias(obj, strings::params, typeBaseName);
			addTypeAlias(obj, strings::partialResult, typeBaseName);
			addTypeAlias(obj, strings::errorData, typeBaseName);
			addTypeAlias(obj, strings::registrationOptions, typeBaseName);
		}

		const auto& notifications = json.get("notifications").array();

		for(const auto& n : notifications)
		{
			const auto& obj = n.object();
			const auto& typeBaseName = obj.get(strings::method).string();
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
#include <lsp/str.h>
#include <lsp/fileuri.h>
#include <lsp/nullable.h>
#include <lsp/json/json.h>
#include <lsp/serialization.h>

namespace lsp{

inline constexpr std::string_view VersionStr{"${LSP_VERSION}"};

using uint      = unsigned int;
using LSPArray  = json::Array;
using LSPObject = json::Object;
using LSPAny    = json::Any;

template<typename K, typename T>
using Map = str::UnorderedMap<K, T>;

)";

static constexpr const char* TypesHeaderEnd =
R"(
} // namespace lsp
)";

static constexpr const char* TypesSourceBegin =
R"(#include "types.h"

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include <cassert>
#include <iterator>
#include <algorithm>

namespace lsp{

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
	const auto begin = values;
	const auto end = values + static_cast<int>(E::MAX_VALUE);
	const auto it = std::lower_bound(begin, end, value);

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
} // namespace lsp
)";

static constexpr const char* MessagesHeaderBegin =
R"(#pragma once

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include "types.h"
#include <lsp/messagebase.h>

namespace lsp{

)";

static constexpr const char* MessagesHeaderEnd =
R"(} // namespace lsp
)";

static constexpr const char* MessagesSourceBegin =
R"(#include "messages.h"

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

namespace lsp{

)";

static constexpr const char* MessagesSourceEnd =
R"(
std::string_view messageMethodToString(MessageMethod method)
{
	return MethodStrings[static_cast<int>(method)];
}

MessageMethod messageMethodFromString(std::string_view str)
{
	auto it = MethodsByString.find(str);

	if(it != MethodsByString.end())
		return it->second;

	return MessageMethod::INVALID;
}

} // namespace lsp
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
		writeFile("types.h", str::replace(TypesHeaderBegin, "${LSP_VERSION}", m_metaModel.metaData().version) + m_typesHeaderFileContent + m_typesBoilerPlateHeaderFileContent + TypesHeaderEnd);
		writeFile("types.cpp", TypesSourceBegin + m_typesSourceFileContent + m_typesBoilerPlateSourceFileContent + TypesSourceEnd);
		writeFile("messages.h", MessagesHeaderBegin + m_messagesHeaderFileContent + MessagesHeaderEnd);
		writeFile("messages.cpp", MessagesSourceBegin + m_messagesSourceFileContent + MessagesSourceEnd);
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
		m_typesBoilerPlateHeaderFileContent = "/*\n * Serialization boilerplate\n */\n\n";
		m_typesBoilerPlateSourceFileContent = m_typesBoilerPlateHeaderFileContent;

		for(const auto& name : m_metaModel.typeNames())
			generateNamedType(name);
	}

	void generateMessages()
	{
		// Message method enum

		m_messagesHeaderFileContent = "enum class MessageMethod{\n"
		                              "\tINVALID = -1,\n\n"
		                              "\t// Requests\n\n";
		m_messagesSourceFileContent = "static constexpr std::string_view MethodStrings[static_cast<int>(MessageMethod::MAX_VALUE)] = {\n";

		std::string messageMethodsByString = "const str::UnorderedMap<std::string_view, MessageMethod> MethodsByString = {\n"
		                                     "#define LSP_MS_PAIR(x) {MethodStrings[static_cast<int>(x)], x}\n";

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Request))
		{
			auto id = upperCaseIdentifier(method);
			m_messagesHeaderFileContent += '\t' + id + ",\n";
			m_messagesSourceFileContent += '\t' + str::quote(method) + ",\n";
			messageMethodsByString += "\tLSP_MS_PAIR(MessageMethod::" + id + "),\n";
		}

		m_messagesHeaderFileContent += "\n\t// Notifications\n\n";

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Notification))
		{
			auto id = upperCaseIdentifier(method);
			m_messagesHeaderFileContent += '\t' + id + ",\n";
			m_messagesSourceFileContent += '\t' + str::quote(method) + ",\n";
			messageMethodsByString += "\tLSP_MS_PAIR(MessageMethod::" + id + "),\n";
		}

		m_messagesHeaderFileContent += "\tMAX_VALUE\n};\n\n";
		m_messagesSourceFileContent.pop_back();
		m_messagesSourceFileContent.pop_back();
		m_messagesSourceFileContent += "\n};\n\n";
		messageMethodsByString.pop_back();
		messageMethodsByString.pop_back();
		messageMethodsByString += "\n#undef LSP_MS_PAIR\n};\n";
		m_messagesSourceFileContent += messageMethodsByString;

		// Structs

		const char* namespaceStr = "/*\n"
		                           " * Request messages\n"
		                           " */\n\n"
		                           "namespace requests{\n\n";

		m_messagesHeaderFileContent += namespaceStr;

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Request))
			generateMessage(method, message, false);

		namespaceStr = "} // namespace requests\n\n"
		               "/*\n"
		               " * Notification messages\n"
		               " */\n\n"
		               "namespace notifications{\n\n";


		m_messagesHeaderFileContent += namespaceStr;

		for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Notification))
			generateMessage(method, message, true);

		namespaceStr = "} // namespace notifications\n";

		m_messagesHeaderFileContent += namespaceStr;
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
		                               "\tstatic constexpr auto Method = MessageMethod::" + messageCppName + ";\n";

		const bool hasRegistrationOptions = !message.registrationOptionsTypeName.empty();
		const bool hasPartialResult = !message.partialResultTypeName.empty();
		const bool hasErrorData = !message.errorDataTypeName.empty();
		const bool hasParams = !message.paramsTypeName.empty();
		const bool hasResult = !message.resultTypeName.empty();

		if(hasRegistrationOptions || hasPartialResult || hasParams || hasResult)
			m_messagesHeaderFileContent += '\n';

		if(hasRegistrationOptions)
			m_messagesHeaderFileContent += "\tusing RegistrationOptions = " + upperCaseIdentifier(message.registrationOptionsTypeName) + ";\n";

		if(hasPartialResult)
			m_messagesHeaderFileContent += "\tusing PartialResult = " + upperCaseIdentifier(message.partialResultTypeName) + ";\n";

		if(hasErrorData)
			m_messagesHeaderFileContent += "\tusing ErrorData = " + upperCaseIdentifier(message.errorDataTypeName) + ";\n";

		if(hasParams)
			m_messagesHeaderFileContent += "\tusing Params = " + upperCaseIdentifier(message.paramsTypeName) + ";\n";

		if(hasResult)
			m_messagesHeaderFileContent += "\tusing Result = " + upperCaseIdentifier(message.resultTypeName) + ";\n";

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

		auto parts = str::splitView(str, "/", true);
		auto id = str::join(parts, "_", [](auto&& s){ return str::capitalize(s); });

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
		return str::uncapitalize(str);
	}

	static std::string toJsonSig(const std::string& typeName)
	{
		return "json::Any toJson(" + typeName + "&& value)";
	}

	static std::string fromJsonSig(const std::string& typeName)
	{
		return "void fromJson(json::Any&& json, " + typeName + "& value)";
	}

	static std::string documentationComment(const std::string& title, const std::string& documentation, int indentLevel = 0)
	{
		std::string indent(indentLevel, '\t');
		std::string comment = indent + "/*\n";

		if(!title.empty())
			comment += indent + " * " + title + "\n";

		auto documentationLines = str::splitView(documentation, "\n");

		if(!documentationLines.empty())
		{
			if(!title.empty())
				comment += indent + " *\n";

			for(const auto& l : documentationLines)
				comment += str::trimRight(indent + " * " + str::replace(str::replace(l, "/*", "/_*"), "*/", "*_/")) + '\n';
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
			                            "\tenum class ValueIndex{\n";
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
			                            valueIndent + str::capitalize(it->name);
			m_typesSourceFileContent += '\t' + json::stringify(it->value);
			++it;

			while(it != enumeration.values.end())
			{
				m_typesHeaderFileContent += ",\n" + documentationComment({}, it->documentation, 1 + enumeration.supportsCustomValues) + valueIndent + str::capitalize(it->name);
				m_typesSourceFileContent += ",\n\t" + json::stringify(it->value);
				++it;
			}
		}

		m_typesHeaderFileContent += ",\n" + valueIndent + "MAX_VALUE\n";

		if(enumeration.supportsCustomValues)
		{
			m_typesHeaderFileContent += "\t};\n"
			                            "\tusing enum ValueIndex;\n\n"
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
			                            "\tm_value = " + enumValuesVarName + "[static_cast<int>(index)];\n"
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
																	           "\treturn toJson(" + baseType.data + "{value.value()});\n}\n\n" +
																	           fromJson + "\n"
																	           "{\n"
																	           "\t" + baseType.data + " jsonVal;\n"
																	           "\tfromJson(std::move(json), jsonVal);\n"
			                                       "\tvalue = jsonVal;\n"
																	           "}\n\n";
		}
		else
		{
			m_typesBoilerPlateSourceFileContent += toJson + "\n"
			                                       "{\n"
																	           "\treturn toJson(" + baseType.data + "{" + enumValuesVarName + "[static_cast<int>(value)]});\n}\n\n" +
																	           fromJson + "\n"
																	           "{\n"
																	           "\t" + baseType.data + " jsonVal;\n"
																	           "\tfromJson(std::move(json), jsonVal);\n"
			                                       "\tvalue = findEnumValue<" + enumerationCppName + ">(" + enumValuesVarName + ", jsonVal);\n"
			                                       "\tif(value == " + enumerationCppName + "::MAX_VALUE)\n"
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
						cppOrType = "NullableVariant<";
					else
						cppOrType = "Nullable<";

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
			typeName += "std::string";
			break;
		case Type::IntegerLiteral:
			typeName += "int";
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
		for(const auto& t : typeList)
		{
			std::string nameSuffix;

			if(t->isA<StructureLiteralType>())
			{
				const auto& structLit = t->as<StructureLiteralType>();

				// Include all non-optional properties in the type name
				for(const auto& p : structLit.properties)
				{
					if(!p.isOptional)
						nameSuffix += '_' + str::capitalize(p.name);
				}
			}

			generateType(t, baseName + nameSuffix);
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
			generateAggregateTypeList(type->as<OrType>().typeList, baseName);
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

	static bool isTrivialBaseType(const TypePtr& type)
	{
		if(type->isA<BaseType>())
		{
			const auto& base = type->as<BaseType>();

			return base.kind == BaseType::Kind::Boolean ||
			       base.kind == BaseType::Kind::Integer ||
			       base.kind == BaseType::Kind::UInteger ||
			       base.kind == BaseType::Kind::Decimal ||
			       base.kind == BaseType::Kind::Null;
		}

		return false;
	}

	void generateStructureProperties(const std::vector<StructureProperty>& properties,
	                                 const std::unordered_map<std::string_view,
	                                 const StructureProperty*>& basePropertiesByName,
	                                 std::string& toJson,
	                                 std::string& fromJson,
	                                 std::vector<std::string>& requiredProperties,
	                                 std::vector<std::pair<std::string_view, std::string>>& literalValues)
	{
		for(const auto& p : properties)
		{
			std::string literalValue;

			if(p.type->isLiteral())
			{
				switch(p.type->category())
				{
				case Type::StringLiteral:
					literalValue = str::quote(str::escape(p.type->as<StringLiteralType>().stringValue));
					break;
				case Type::IntegerLiteral:
					literalValue = std::to_string(p.type->as<IntegerLiteralType>().integerValue);
					break;
				case Type::BooleanLiteral:
					literalValue = std::string{p.type->as<BooleanLiteralType>().booleanValue ? "true" : "false"};
					break;
				default:
					break;
				}

				if(basePropertiesByName.contains(p.name))
				{
					literalValues.emplace_back(p.name, std::move(literalValue));
					continue; // Don't write literal properties with the same name as an inherited property. Instead initialize the inherited property.
				}
			}

			std::string typeName = cppTypeName(*p.type, p.isOptional);

			m_typesHeaderFileContent += documentationComment({}, p.documentation, 1) +
			                            '\t' + typeName + ' ' + p.name;

			if(!literalValue.empty())
				m_typesHeaderFileContent += " = " + literalValue;
			// Provide default initializer for optional values
			else if (p.isOptional)
				m_typesHeaderFileContent += " = {}";


			m_typesHeaderFileContent += ";\n";

			if(p.isOptional)
			{
				toJson += "\tif(value." + p.name + ")\n\t";
				fromJson += "\tif(const auto it = json.find(\"" + p.name + "\"); it != json.end())\n\t"
				            "\tfromJson(std::move(it->second), value." + p.name + ");\n";
			}
			else
			{
				requiredProperties.push_back(p.name);
				fromJson += "\tfromJson(std::move(json.get(\"" + p.name + "\")), value." + p.name + ");\n";
			}

			if(isTrivialBaseType(p.type))
				toJson += "\tjson[\"" + p.name + "\"] = toJson(" + typeName + "{value." + p.name + "});\n";
			else
				toJson += "\tjson[\"" + p.name + "\"] = toJson(std::move(value." + p.name + "));\n";
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
				generateType(p.type, structureCppName + str::capitalize(p.name));
		}

		m_typesHeaderFileContent += documentationComment(structureCppName, structure.documentation) +
		                            "struct " + structureCppName;

		std::string propertiesToJson = "static void " + str::uncapitalize(structureCppName) + "ToJson(" +
		                               structureCppName + "&& value, json::Object& json)\n{\n";
		std::string propertiesFromJson = "static void " + str::uncapitalize(structureCppName) + "FromJson("
		                                 "json::Object& json, " + structureCppName + "& value)\n{\n";
		std::string requiredPropertiesSig = "template<>\nconst char** requiredProperties<" + structureCppName + ">()";
		std::vector<std::string> requiredPropertiesList;
		std::string requiredProperties = requiredPropertiesSig + "\n{\n\tstatic const char* properties[] = {\n";

		// Add base classes

		std::unordered_map<std::string_view, const StructureProperty*> basePropertiesByName;
		if(auto it = structure.extends.begin(); it != structure.extends.end())
		{
			const auto* extends = &(*it)->as<ReferenceType>();
			m_typesHeaderFileContent += " : " + extends->name;
			std::string lower = str::uncapitalize(extends->name);
			propertiesToJson += '\t' + lower + "ToJson(std::move(value), json);\n";
			propertiesFromJson += '\t' + lower + "FromJson(json, value);\n";
			++it;

			for(const auto& p : std::get<const Structure*>(m_metaModel.typeForName(extends->name))->properties)
			{
				if(!p.isOptional)
					requiredPropertiesList.push_back(p.name);

				basePropertiesByName[p.name] = &p;
			}

			while(it != structure.extends.end())
			{
				extends = &(*it)->as<ReferenceType>();
				m_typesHeaderFileContent += ", " + extends->name;
				lower = str::uncapitalize(extends->name);
				propertiesToJson += '\t' + lower + "ToJson(std::move(value), json);\n";
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

		std::vector<std::pair<std::string_view, std::string>> literalValues;
		for(const auto& m : structure.mixins)
		{
			if(!m->isA<ReferenceType>())
				throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a type reference"};

			auto type = m_metaModel.typeForName(m->as<ReferenceType>().name);

			if(!std::holds_alternative<const Structure*>(type))
				throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a structure type"};

			generateStructureProperties(std::get<const Structure*>(type)->properties, basePropertiesByName, propertiesToJson, propertiesFromJson, requiredPropertiesList, literalValues);
		}

		generateStructureProperties(structure.properties, basePropertiesByName, propertiesToJson, propertiesFromJson, requiredPropertiesList, literalValues);

		if(!literalValues.empty())
		{
			m_typesHeaderFileContent += "\n\t" + structure.name + "()\n\t{\n";

			for(const auto& v : literalValues)
			{
				m_typesHeaderFileContent += "\t\t";
				m_typesHeaderFileContent += v.first;
				m_typesHeaderFileContent += " = " + v.second + ";\n";
			}

			m_typesHeaderFileContent += "\t}\n";
		}

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
		                                       "\t" + str::uncapitalize(structureCppName) + "ToJson(std::move(value), obj);\n"
		                                       "\treturn obj;\n"
		                                       "}\n\n" +
		                                       fromJson + "\n"
		                                       "{\n"
		                                       "\tauto& obj = json.object();\n"
		                                       "\t" + str::uncapitalize(structureCppName) + "FromJson(obj, value);\n"
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
	{"uint", "uint", "uint", "uint"},
	{"double", "double", "double", "double"},
	{"FileURI", "FileURI", "const FileURI&", "const FileURI&"},
	{"FileURI", "FileURI", "const FileURI&", "const FileURI&"},
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
			auto json = json::parse(jsonText).object();
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
