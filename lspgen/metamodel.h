#pragma once

#include <map>
#include <memory>
#include <variant>
#include <lsp/json/json.h>

namespace lspgen{

namespace json = lsp::json;

class MetaModel;

/*
 * Type
 */

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

	virtual auto category() const -> Category = 0;
	virtual void extract(const json::Object& json) = 0;

	auto isLiteral() const -> bool;
	static auto categoryFromString(std::string_view str) -> Category;

	template<typename T>
	auto isA() const -> bool
	{
		return dynamic_cast<const T*>(this) != nullptr;
	}

	template<typename T>
	auto as() -> T&
	{
		return dynamic_cast<T&>(*this);
	}

	template<typename T>
	auto as() const -> const T&
	{
		return dynamic_cast<const T&>(*this);
	}

	static auto createFromJson(const json::Object& json) -> TypePtr;
};

/*
 * BaseType
 */

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

	auto category() const -> Category override{ return Category::Base; }

	void extract(const json::Object& json) override;
	static auto kindFromString(std::string_view str) -> Kind;
};

/*
 * ReferenceType
 */

struct ReferenceType : Type{
	std::string name;

	auto category() const -> Category override{ return Category::Reference; }
	void extract(const json::Object& json) override;
};

/*
 * ArrayType
 */

struct ArrayType : Type{
	TypePtr elementType;

	auto category() const -> Category override{ return Category::Array; }
	void extract(const json::Object& json) override;
};

/*
 * MapType
 */

struct MapType : Type{
	TypePtr keyType;
	TypePtr valueType;

	auto category() const -> Category override{ return Category::Map; }
	void extract(const json::Object& json) override;
};

/*
 * AndType
 */

struct AndType : Type{
	std::vector<TypePtr> typeList;

	auto category() const -> Category override{ return Category::And; }
	void extract(const json::Object& json) override;
};

/*
 * OrType
 */

struct OrType : Type{
	std::vector<TypePtr> typeList;

	auto category() const -> Category override{ return Category::Or; }
	void extract(const json::Object& json) override;
};

/*
 * TupleType
 */

struct TupleType : Type{
	std::vector<TypePtr> typeList;

	auto category() const -> Category override{ return Category::Tuple; }
	void extract(const json::Object& json) override;
};

/*
 * StructureProperty
 */

struct StructureProperty{
	std::string name;
	TypePtr     type;
	bool        isOptional = false;
	std::string documentation;

	void extract(const json::Object& json);
};

using StructurePropertyList = std::vector<StructureProperty>;

auto extractStructureProperties(const json::Array& json) -> StructurePropertyList;

/*
 * StructureLiteralType
 */

struct StructureLiteralType : Type{
	StructurePropertyList properties;

	auto category() const -> Category override{ return Category::StructureLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * StructureLiteralType
 */

struct StringLiteralType : Type{
	std::string stringValue;

	auto category() const -> Category override{ return Category::StringLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * IntegerLiteralType
 */

struct IntegerLiteralType : Type{
	json::Integer integerValue = 0;

	auto category() const -> Category override{ return Category::IntegerLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * BooleanLiteralType
 */

struct BooleanLiteralType : Type{
	bool booleanValue = false;

	auto category() const -> Category override{ return Category::BooleanLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * Enumeration
 */

struct Enumeration{
	std::string name;
	TypePtr     type;

	struct Value{
		std::string name;
		json::Value value;
		std::string documentation;
	};

	std::vector<Value> values;
	std::string        documentation;
	bool               supportsCustomValues = false;

	void extract(const json::Object& json);
};

/*
 * Structure
 */

struct Structure{
	std::string name;
	std::vector<StructureProperty> properties;
	std::vector<TypePtr>  extends;
	std::vector<TypePtr>  mixins;
	std::string           documentation;

	void extract(const json::Object& json);
	auto findBaseProperty(std::string_view name, const MetaModel& metaModel) const -> const StructureProperty*;
	auto findProperty(std::string_view name, const MetaModel& metaModel) const -> const StructureProperty*;
};

/*
 * TypeAlias
 */

struct TypeAlias{
	std::string name;
	TypePtr     type;
	std::string documentation;

	void extract(const json::Object& json);
};

/*
 * Message
 */

struct Message{
	enum class Direction{
		ClientToServer,
		ServerToClient,
		Both
	};

	std::string documentation;
	std::string clientCapabilityName;
	std::string serverCapabilityName;
	Direction   direction;

	// Those should be omitted if the strings are empty
	std::string paramsTypeName;
	std::string resultTypeName;
	std::string partialResultTypeName;
	std::string errorDataTypeName;
	std::string registrationOptionsTypeName;

	void extract(const json::Object& json);
};

/*
 * MetaModel
 */

class MetaModel{
public:
	MetaModel();
	MetaModel(const json::Object& json);

	using TypeVariant = std::variant<const Enumeration*, const Structure*, const TypeAlias*>;

	void extract(const json::Object& json);
	auto typeForName(std::string_view name) const -> TypeVariant;

	enum class MessageType{
		Request,
		Notification
	};

	auto messagesByType(MessageType type) const -> const std::map<std::string, Message>&;

	struct MetaData{
		std::string version;
	};

	auto metaData() const -> const MetaData&{ return m_metaData; }
	auto typeNames() const -> const std::vector<std::string_view>&{ return m_typeNames; }
	auto enumerations() const -> const std::vector<Enumeration>&{ return m_enumerations; }
	auto structures() const -> const std::vector<Structure>&{ return m_structures; }
	auto typeAliases() const -> const std::vector<TypeAlias>&{ return m_typeAliases; }

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

	void extractMetaData(const json::Object& json);
	void extractTypes(const json::Object& json);
	void extractMessages(const json::Object& json);
	void extractRequests(const json::Object& json);
	void extractNotifications(const json::Object& json);
	void insertType(const std::string& name, Type type, std::size_t index);
	void extractEnumerations(const json::Object& json);
	void extractStructures(const json::Object& json);
	void addTypeAlias(const json::Object& json, const std::string& key, const std::string& typeBaseName);
	void extractTypeAliases(const json::Object& json);
};

} // namespace lspgen
