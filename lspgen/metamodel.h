#pragma once

#include <map>
#include <memory>
#include <variant>
#include <lsp/json/json.h>

namespace lspgen{

namespace json = lsp::json;

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

	virtual Category category() const = 0;
	virtual void extract(const json::Object& json) = 0;

	bool isLiteral() const;
	static Category categoryFromString(std::string_view str);

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

	Category category() const override{ return Category::Base; }

	void extract(const json::Object& json) override;
	static Kind kindFromString(std::string_view str);
};

/*
 * ReferenceType
 */

struct ReferenceType : Type{
	std::string name;

	Category category() const override{ return Category::Reference; }
	void extract(const json::Object& json) override;
};

/*
 * ArrayType
 */

struct ArrayType : Type{
	TypePtr elementType;

	Category category() const override{ return Category::Array; }
	void extract(const json::Object& json) override;
};

/*
 * MapType
 */

struct MapType : Type{
	TypePtr keyType;
	TypePtr valueType;

	Category category() const override{ return Category::Map; }
	void extract(const json::Object& json) override;
};

/*
 * AndType
 */

struct AndType : Type{
	std::vector<TypePtr> typeList;

	Category category() const override{ return Category::And; }
	void extract(const json::Object& json) override;
};

/*
 * OrType
 */

struct OrType : Type{
	std::vector<TypePtr> typeList;

	Category category() const override{ return Category::Or; }
	void extract(const json::Object& json) override;
};

/*
 * TupleType
 */

struct TupleType : Type{
	std::vector<TypePtr> typeList;

	Category category() const override{ return Category::Tuple; }
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

StructurePropertyList extractStructureProperties(const json::Array& json);

/*
 * StructureLiteralType
 */

struct StructureLiteralType : Type{
	StructurePropertyList properties;

	Category category() const override{ return Category::StructureLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * StructureLiteralType
 */

struct StringLiteralType : Type{
	std::string stringValue;

	Category category() const override{ return Category::StringLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * IntegerLiteralType
 */

struct IntegerLiteralType : Type{
	json::Integer integerValue = 0;

	Category category() const override{ return Category::IntegerLiteral; }
	void extract(const json::Object& json) override;
};

/*
 * BooleanLiteralType
 */

struct BooleanLiteralType : Type{
	bool booleanValue = false;

	Category category() const override{ return Category::BooleanLiteral; }
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
	MetaModel() = default;

	using TypeVariant = std::variant<const Enumeration*, const Structure*, const TypeAlias*>;

	void extract(const json::Object& json);
	TypeVariant typeForName(std::string_view name) const;

	enum class MessageType{
		Request,
		Notification
	};

	const std::map<std::string, Message>& messagesByName(MessageType type) const;

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
