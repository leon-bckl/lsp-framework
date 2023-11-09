#include <cstdlib>
#include <fstream>
#include <iostream>
#include <lsp/json/json.h>
#include <lsp/util/util.h>
#include <unordered_set>
#include <cassert>
#include <memory>

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
STRING(mixins)
STRING(name)
STRING(optional)
STRING(properties)
STRING(supportsCustomValues)
STRING(type)
STRING(value)
STRING(values)
#undef STRING

};

std::string extractDocumentation(const json::Object& json){
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

	static constexpr std::string_view TypeCategoryStrings[] = {
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

	static Category categoryFromString(std::string_view str){
		for(std::size_t i = 0; i < std::size(TypeCategoryStrings); ++i){
			if(TypeCategoryStrings[i] == str)
				return static_cast<Type::Category>(i);
		}

		throw std::runtime_error{'\'' + std::string{str} + "' is not a valid type kind"};
	}

	template<typename T>
	bool isA() const{
		return dynamic_cast<const T*>(this) != nullptr;
	}

	template<typename T>
	T& as(){
		return dynamic_cast<T&>(*this);
	}

	template<typename T>
	const T& as() const{
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

	static constexpr std::string_view BaseTypeStrings[] = {
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

	void extract(const json::Object& json) override{
		kind = kindFromString(json.get<json::String>(strings::name));
	}

	static Kind kindFromString(std::string_view str){
		for(std::size_t i = 0; i < std::size(BaseTypeStrings); ++i){
			if(BaseTypeStrings[i] == str)
				return static_cast<Kind>(i);
		}

		throw std::runtime_error{'\'' + std::string{str} + "' is not a valid base type"};
	}
};

struct ReferenceType : Type{
	std::string name;

	Category category() const override{ return Category::Reference; }

	void extract(const json::Object& json) override{
		name = json.get<json::String>(strings::name);
	}
};

struct ArrayType : Type{
	TypePtr elementType;

	Category category() const override{ return Category::Array; }

	void extract(const json::Object& json) override{
		const auto& elementTypeJson = json.get<json::Object>(strings::element);
		elementType = createFromJson(elementTypeJson);
	}
};

struct MapType : Type{
	TypePtr keyType;
	TypePtr valueType;

	void extract(const json::Object& json) override{
		keyType = createFromJson(json.get<json::Object>(strings::key));
		valueType = createFromJson(json.get<json::Object>(strings::value));
	}

	Category category() const override{ return Category::Map; }
};

struct AndType : Type{
	std::vector<TypePtr> typeList;

	void extract(const json::Object& json) override{
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

	void extract(const json::Object& json) override{
		const auto& items = json.get<json::Array>(strings::items);
		typeList.reserve(items.size());

		for(const auto& i : items)
			typeList.push_back(createFromJson(i.get<json::Object>()));
	}

	Category category() const override{ return Category::Tuple; }
};

struct StructureLiteralType : Type{
	struct Property{
		std::string name;
		TypePtr     type;
		bool        isOptional = false;
		std::string documentation;
	};

	std::vector<Property> properties;

	void extract(const json::Object& json) override{
		const auto& value = json.get<json::Object>(strings::value);
		const auto& propertiesJson = value.get<json::Array>(strings::properties);
		properties.reserve(propertiesJson.size());

		for(const auto& p : propertiesJson){
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

static TypePtr mergeStructureLiterals(std::vector<std::unique_ptr<Type>>& structureLiteralTypes){
	struct PropertyInfo{
		std::vector<TypePtr> structureLiteralTypes;
		std::vector<TypePtr> otherTypes;
		std::string documentation;
		bool isOptional = false;
	};

	std::unordered_map<std::string, PropertyInfo> propertyTypeMap;

	for(const auto& t : structureLiteralTypes){
		auto& s = t->as<StructureLiteralType>();

		for(auto& p : s.properties){
			auto& info = propertyTypeMap[p.name];

			if(p.type->isA<StructureLiteralType>()){
				info.structureLiteralTypes.push_back(std::move(p.type));
			}else{
				bool exists = false;

				for(const auto& o : info.otherTypes){
					if(o->category() == p.type->category()){
						exists = true;
						break;
					}
				}

				if(!exists)
					info.otherTypes.push_back(std::move(p.type));
			}

			if(!p.documentation.empty())
				info.documentation = p.documentation;

			info.isOptional |= p.isOptional;
		}
	}

	structureLiteralTypes.clear();

	auto newType = std::make_unique<StructureLiteralType>();

	for(auto& [name, info] : propertyTypeMap){
		auto& prop = newType->properties.emplace_back();
		prop.name = name;
		prop.documentation = info.documentation;
		prop.isOptional = info.isOptional;

		TypePtr newPropType;

		if(!info.structureLiteralTypes.empty()){
			if(info.structureLiteralTypes.size() == 1){
				newPropType = std::move(info.structureLiteralTypes[0]);
				info.structureLiteralTypes.clear();
			}else{
				newPropType = mergeStructureLiterals(info.structureLiteralTypes);
			}
		}

		if((newPropType && !info.otherTypes.empty()) || info.otherTypes.size() > 1){
			auto orType = std::make_unique<OrType>();

			if(newPropType)
				orType->typeList.push_back(std::move(newPropType));

			for(auto& o : info.otherTypes)
				orType->typeList.push_back(std::move(o));

			prop.type = std::move(orType);
		}else{
			assert(newPropType || info.otherTypes.size() == 1);

			if(newPropType)
				prop.type = std::move(newPropType);
			else
				prop.type = std::move(info.otherTypes[0]);
		}
	}

	return newType;
}

void OrType::extract(const json::Object& json){
	const auto& items = json.get<json::Array>(strings::items);
	typeList.reserve(items.size());

	std::vector<std::unique_ptr<Type>> structureLiterals;

	for(const auto& item : items){
		auto type = createFromJson(item.get<json::Object>());

		if(type->isA<StructureLiteralType>())
			structureLiterals.push_back(std::move(type));
		else
			typeList.push_back(std::move(type));
	}

	if(!structureLiterals.empty())
		typeList.push_back(mergeStructureLiterals(structureLiterals));

	if(typeList.empty())
		throw std::runtime_error{"OrType must not be empty!"};
}

struct StringLiteralType : Type{
	std::string stringValue;

	void extract(const json::Object& json) override{
		stringValue = static_cast<json::String>(json.get<json::String>(strings::value));
	}

	Category category() const override{ return Category::StringLiteral; }
};

struct IntegerLiteralType : Type{
	json::Integer integerValue = 0;

	void extract(const json::Object& json) override{
		integerValue = static_cast<json::Integer>(json.get(strings::value).numberValue());
	}

	Category category() const override{ return Category::IntegerLiteral; }
};

struct BooleanLiteralType : Type{
	bool booleanValue = false;

	void extract(const json::Object& json) override{
		booleanValue = json.get<json::Boolean>(strings::value);
	}

	Category category() const override{ return Category::BooleanLiteral; }
};

TypePtr Type::createFromJson(const json::Object& json){
	TypePtr result;
	auto category = categoryFromString(json.get<json::String>(strings::kind));

	switch(category){
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

	void extract(const json::Object& json){
		name = json.get<json::String>(strings::name);
		const auto& typeJson = json.get<json::Object>(strings::type);
		type = Type::createFromJson(typeJson);
		const auto& valuesJson = json.get<json::Array>(strings::values);
		values.reserve(valuesJson.size());

		for(const auto& v : valuesJson){
			const auto& obj = v.get<json::Object>();
			auto& enumValue = values.emplace_back();
			enumValue.name = obj.get<json::String>(strings::name);
			enumValue.value = obj.get(strings::value);
			enumValue.documentation = extractDocumentation(obj);
		}

		documentation = extractDocumentation(json);

		if(json.contains(strings::supportsCustomValues))
			supportsCustomValues = json.get<bool>(strings::supportsCustomValues);
	}
};

struct Structure{
	std::string name;

	struct Property{
		std::string name;
		TypePtr     type;
		bool        isOptional = false;
		std::string documentation;
	};

	std::vector<Property> properties;
	std::vector<TypePtr>  extends;
	std::vector<TypePtr>  mixins;
	std::string           documentation;

	void extract(const json::Object& json){
		name = json.get<json::String>(strings::name);
		const auto& propertiesJson = json.get<json::Array>(strings::properties);
		properties.reserve(propertiesJson.size());

		for(const auto& p : propertiesJson){
			const auto& obj = p.get<json::Object>();
			auto& property = properties.emplace_back();
			property.name = obj.get<json::String>(strings::name);
			property.type = Type::createFromJson(obj.get<json::Object>(strings::type));

			if(obj.contains(strings::optional))
				property.isOptional = obj.get<json::Boolean>(strings::optional);

			property.documentation = extractDocumentation(obj);
		}

		if(json.contains(strings::extends)){
			const auto& extendsJson = json.get<json::Array>(strings::extends);
			extends.reserve(extendsJson.size());

			for(const auto& e : extendsJson)
				extends.push_back(Type::createFromJson(e.get<json::Object>()));
		}

		if(json.contains(strings::mixins)){
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

	void extract(const json::Object& json){
		name = json.get<json::String>(strings::name);
		type = Type::createFromJson(json.get<json::Object>(strings::type));
		documentation = extractDocumentation(json);
	}
};

class MetaModel{
public:
	MetaModel() = default;

	void extract(const json::Object& json){
		extractMetaData(json);
		extractTypes(json);
	}

	std::variant<const Enumeration*, const Structure*, const TypeAlias*> typeForName(std::string_view name) const{
		if(auto it = m_typesByName.find(std::string{name}); it != m_typesByName.end()){
			switch(it->second.type){
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

	void extractMetaData(const json::Object& json){
		const auto& metaDataJson = json.get<json::Object>("metaData");
		m_metaData.version = metaDataJson.get<json::String>("version");
	}

	void extractTypes(const json::Object& json){
		extractEnumerations(json);
		extractTypeAliases(json);
		extractStructures(json);
	}

	void insertType(const std::string& name, Type type, std::size_t index){
		if(m_typesByName.contains(name))
			throw std::runtime_error{"Duplicate type '" + name + '\"'};

		auto it = m_typesByName.insert(std::pair(name, TypeIndex{type, index})).first;
		m_typeNames.push_back(it->first);
	}

	void extractEnumerations(const json::Object& json){
		const auto& enumerations = json.get<json::Array>("enumerations");

		m_enumerations.resize(enumerations.size());

		for(std::size_t i = 0; i < enumerations.size(); ++i){
			m_enumerations[i].extract(enumerations[i].get<json::Object>());
			insertType(m_enumerations[i].name, Type::Enumeration, i);
		}
	}

	void extractTypeAliases(const json::Object& json){
		const auto& typeAliases = json.get<json::Array>("typeAliases");

		m_typeAliases.resize(typeAliases.size());

		for(std::size_t i = 0; i < typeAliases.size(); ++i){
			m_typeAliases[i].extract(typeAliases[i].get<json::Object>());
			insertType(m_typeAliases[i].name, Type::TypeAlias, i);
		}
	}

	void extractStructures(const json::Object& json){
		const auto& structures = json.get<json::Array>("structures");

		m_structures.resize(structures.size());

		for(std::size_t i = 0; i < structures.size(); ++i){
			m_structures[i].extract(structures[i].get<json::Object>());
			insertType(m_structures[i].name, Type::Structure, i);
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
#include <lsp/json/json.h>

namespace lsp{

inline constexpr std::string_view VersionStr{"${LSP_VERSION}"};

using LSPArray  = json::Array;
using LSPObject = json::Object;
using LSPAny    = json::Any;

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

namespace lsp{
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

)";

static constexpr const char* TypesSourceEnd =
R"(#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
} // namespace lsp
)";

class CppGenerator{
public:
	CppGenerator(const MetaModel* metaModel) : m_metaModel{*metaModel}{}

	void generateTypes(){
		m_processedTypes = {"LSPArray", "LSPObject", "LSPAny"};
		m_typesBeingProcessed = {};
		m_typesHeaderFileContent = util::str::replace(TypesHeaderBegin, "${LSP_VERSION}", m_metaModel.metaData().version);
		m_typesHeaderFileContentBoilerPlate = "/*\n * Boiler plate\n */\n\n";
		m_typesSourceFileContent = TypesSourceBegin;

		for(const auto& name : m_metaModel.typeNames())
			generateNamedType(name);

		m_typesHeaderFileContentBoilerPlate += TypesHeaderEnd;
		m_typesSourceFileContent += TypesSourceEnd;
	}

	void writeFiles(){
		writeFile("types.h", m_typesHeaderFileContent + m_typesHeaderFileContentBoilerPlate);
		writeFile("types.cpp", m_typesSourceFileContent);
	}

private:
	std::string                                  m_typesHeaderFileContent;
	std::string                                  m_typesHeaderFileContentBoilerPlate;
	std::string                                  m_typesSourceFileContent;
	const MetaModel&                             m_metaModel;
	std::unordered_set<std::string_view>         m_processedTypes;
	std::unordered_set<std::string_view>         m_typesBeingProcessed;
	std::unordered_map<const Type*, std::string> m_generatedTypeNames;

	struct CppBaseType{
		std::string data;
		std::string constData;
		std::string param;
		std::string getResult;
	};

	static const CppBaseType s_stringBaseType;
	static const CppBaseType s_baseTypeMapping[BaseType::MAX];

	static void writeFile(std::string_view name, std::string_view content){
		std::ofstream file{name, std::ios::trunc | std::ios::binary};
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	static std::string toJsonSig(const std::string& typeName){
		return "template<>\njson::Any toJson(const " + typeName + "& value)";
	}

	static std::string fromJsonSig(const std::string& typeName){
		return "template<>\nvoid fromJson(const json::Any& json, " + typeName + "& value)";
	}

	static std::string documentationComment(const std::string& title, const std::string& documentation, int indentLevel = 0){
		std::string indent(indentLevel, '\t');
		std::string comment = indent + "/*\n";

		if(!title.empty())
			comment += indent + " * " + title + "\n";

		auto documentationLines = util::str::splitView(documentation, "\n");

		if(!documentationLines.empty()){
			if(!title.empty())
				comment += indent + " *\n";

			for(const auto& l : documentationLines)
				comment += indent + " * " + util::str::replace(util::str::replace(l, "/*", "/_*"), "*/", "*_/") + '\n';
		}else if(title.empty()){
		 return {};
		}

		comment += indent + " */\n";

		return comment;
	}

	void generateNamedType(std::string_view name){
		if(m_processedTypes.contains(name))
			return;

		m_processedTypes.insert(name);
		std::visit([this](const auto* ptr){
			m_typesBeingProcessed.insert(ptr->name);
			generate(*ptr);
			m_typesBeingProcessed.erase(ptr->name);
		}, m_metaModel.typeForName(name));
	}

	void generate(const Enumeration& enumeration){
		if(!enumeration.type->isA<BaseType>())
			throw std::runtime_error{"Enumeration value type must be a base type"};

		const auto& baseType = s_baseTypeMapping[enumeration.type->as<BaseType>().kind];
		std::string enumValuesVarName = util::str::uncapitalize(enumeration.name) + "EnumValues";

		m_typesHeaderFileContent += documentationComment(enumeration.name, enumeration.documentation);
		std::string valueIndent;

		if(enumeration.supportsCustomValues){
			m_typesHeaderFileContent += "class " + enumeration.name + "{\n"
			                            "public:\n"
			                            "\tenum ValueIndex{\n";
			valueIndent = "\t\t";
		}else{
			m_typesHeaderFileContent += "enum class " + enumeration.name + "{\n";
			valueIndent = "\t";
		}

		m_typesSourceFileContent += "static const " +
		                             baseType.constData + ' ' + enumValuesVarName +
		                             "[static_cast<int>(" + enumeration.name + "::MAX_VALUE)] = {\n";

		if(auto it = enumeration.values.begin(); it != enumeration.values.end()){
			m_typesHeaderFileContent += documentationComment({}, it->documentation, 1) +
			                            valueIndent + util::str::capitalize(it->name);
			m_typesSourceFileContent += '\t' + json::stringify(it->value);
			++it;

			while(it != enumeration.values.end()){
				m_typesHeaderFileContent += ",\n" + valueIndent + util::str::capitalize(it->name);
				m_typesSourceFileContent += ",\n\t" + json::stringify(it->value);
				++it;
			}
		}

		m_typesHeaderFileContent += ",\n" + valueIndent + "MAX_VALUE\n";

		if(enumeration.supportsCustomValues){
			m_typesHeaderFileContent += "\t};\n\n"
			                            "\t" + enumeration.name + "() = default;\n" +
			                            "\t" + enumeration.name + "(ValueIndex index);\n" +
			                            "\t" + enumeration.name + "& operator=(ValueIndex index);\n"
			                            "\t" + enumeration.name + "& operator=(" + baseType.param + " newValue){ m_index = MAX_VALUE; m_value = newValue; return *this; }\n"
																	"\tbool operator==(ValueIndex index) const{ return m_index == index; }\n"
			                            "\toperator const " + baseType.data + "&() const{ return m_value; }\n"
			                            "\t" + baseType.getResult + " value() const{ return m_value; }\n\n"
			                            "private:\n"
			                            "\tValueIndex m_index = MAX_VALUE;\n"
			                            "\t" + baseType.data + " m_value{};\n"
			                            "};\n\n";
		}else{
			m_typesHeaderFileContent += "};\n\n";
		}

		m_typesSourceFileContent += "\n};\n\n";

		auto toJson = toJsonSig(enumeration.name);
		auto fromJson = fromJsonSig(enumeration.name);

		m_typesHeaderFileContentBoilerPlate += toJson + ";\n" +
		                                       fromJson + ";\n";

	  if(enumeration.supportsCustomValues){
			m_typesSourceFileContent += enumeration.name + "::" + enumeration.name + "(ValueIndex index){\n\t*this = index;\n}\n\n" +
			                            enumeration.name + "& " + enumeration.name + "::operator=(ValueIndex index){\n"
			                            "\tassert(index < MAX_VALUE);\n"
			                            "\tm_index = index;\n"
			                            "\tm_value = " + enumValuesVarName + "[index];\n"
			                            "\treturn *this;\n"
			                            "}\n\n" +
			                            toJson + "{\n" +
																	"\treturn value.value();\n}\n\n" +
																	fromJson + "{\n"
																	"\tconst auto& jsonVal = json.get<" + baseType.data + ">();\n"
																	"\tfor(std::size_t i = 0; i < std::size(" + enumValuesVarName + "); ++i){\n"
																	"\t\tif(jsonVal == " + enumValuesVarName + "[i]){\n"
																	"\t\t\tvalue = static_cast<" + enumeration.name + "::ValueIndex>(i);\n"
																	"\t\t\treturn;\n"
																	"\t\t}\n"
																	"\t}\n\n"
																	"\tvalue = jsonVal;\n"
																	"}\n\n";
		}else{
			m_typesSourceFileContent += toJson + "{\n" +
																	"\treturn " + baseType.data + "{" + enumValuesVarName + "[static_cast<int>(value)]};\n}\n\n" +
																	fromJson + "{\n"
																	"\tconst auto& jsonVal = json.get<" + baseType.data + ">();\n"
																	"\tfor(std::size_t i = 0; i < std::size(" + enumValuesVarName + "); ++i){\n"
																	"\t\tif(jsonVal == " + enumValuesVarName + "[i]){\n"
																	"\t\t\tvalue = static_cast<" + enumeration.name + ">(i);\n"
																	"\t\t\treturn;\n"
																	"\t\t}\n"
																	"\t}\n\n"
																	"\tthrow json::TypeError{\"Invalid value for '" + enumeration.name + "'\"};\n"
																	"}\n\n";
		}
	}

	std::string cppTypeName(const TypePtr& type, bool optional = false){
		std::string typeName;

		if(optional){
			if(!type->isA<ReferenceType>() || !m_typesBeingProcessed.contains(type->as<ReferenceType>().name))
				typeName = "std::optional<";
			else
				typeName = "std::unique_ptr<";
		}

		switch(type->category()){
		case Type::Base:
			typeName += s_baseTypeMapping[static_cast<int>(type->as<BaseType>().kind)].data;
			break;
		case Type::Reference:
			typeName += type->as<ReferenceType>().name;
			break;
		case Type::Array:
			{
				const auto& arrayType = type->as<ArrayType>();
				if(arrayType.elementType->isA<ReferenceType>() && arrayType.elementType->as<ReferenceType>().name == "LSPAny")
					typeName += "LSPArray";
				else
					typeName += "std::vector<" + cppTypeName(arrayType.elementType) + '>';

				break;
			}
		case Type::Map:
			typeName += "std::unordered_map<" + cppTypeName(type->as<MapType>().keyType) + ", " + cppTypeName(type->as<MapType>().valueType)  + '>';
			break;
		case Type::And:
			{
				std::string cppTupleType = "std::tuple<";
				const auto& andType = type->as<AndType>();

				if(auto it = andType.typeList.begin(); it != andType.typeList.end()){
					cppTupleType += cppTypeName(*it);
					++it;

					while(it != andType.typeList.end()){
						cppTupleType += ", " + cppTypeName(*it);
						++it;
					}
				}

				cppTupleType += '>';

				typeName += cppTupleType;
				break;
			}
		case Type::Or:
			{
				const auto& orType = type->as<OrType>();

				if(orType.typeList.size() > 1){
					std::string cppOrType = "std::variant<";

					if(auto it = orType.typeList.begin(); it != orType.typeList.end()){
						cppOrType += cppTypeName(*it);
						++it;

						while(it != orType.typeList.end()){
							cppOrType += ", " + cppTypeName(*it);
							++it;
						}
					}

					cppOrType += '>';

					typeName += cppOrType;
					break;
				}else{
					assert(!orType.typeList.empty());
					typeName += cppTypeName(orType.typeList[0]);
				}

				break;
			}
		case Type::Tuple:
			{
				std::string cppTupleType = "std::tuple<";
				const auto& tupleType = type->as<TupleType>();

				if(auto it = tupleType.typeList.begin(); it != tupleType.typeList.end()){
					cppTupleType += cppTypeName(*it);
					++it;

					while(it != tupleType.typeList.end()){
						cppTupleType += ", " + cppTypeName(*it);
						++it;
					}
				}

				cppTupleType += '>';

				typeName += cppTupleType;
				break;
			}
		case Type::StructureLiteral:
			typeName += m_generatedTypeNames[type.get()];
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

	void generateType(const TypePtr& type, const std::string& alternateName){
		switch(type->category()){
		case Type::Reference:
			generateNamedType(type->as<ReferenceType>().name);
			break;
		case Type::Array:
			generateType(type->as<ArrayType>().elementType, alternateName);
			break;
		case Type::Map:
			generateType(type->as<MapType>().keyType, alternateName);
			generateType(type->as<MapType>().valueType, alternateName);
			break;
		case Type::And:
			{
				int i = 0;
				for(const auto& t : type->as<AndType>().typeList){
					generateType(t, alternateName + (i > 0 ? std::to_string(i) : ""));
					i += t->isA<StructureLiteralType>();
				}

				break;
			}
		case Type::Or:
			{
				int i = 0;
				for(const auto& t : type->as<OrType>().typeList){
					generateType(t, alternateName + (i > 0 ? std::to_string(i) : ""));
					i += t->isA<StructureLiteralType>();
				}

				break;
			}
		case Type::Tuple:
			{
				int i = 0;
				for(const auto& t : type->as<TupleType>().typeList){
					generateType(t, alternateName + (i > 0 ? std::to_string(i) : ""));
					i += t->isA<StructureLiteralType>();
				}

				break;
			}
		case Type::StructureLiteral:
			{
				// HACK:
				// Temporarily transfer ownership of property types to temporary structure so the same generator code can be used for structure literals.
				auto& structureLiteral = type->as<StructureLiteralType>();
				Structure structure;

				structure.name = alternateName;
				m_generatedTypeNames[type.get()] = structure.name;
				structure.properties.reserve(structureLiteral.properties.size());

				for(auto& p : structureLiteral.properties){
					auto& prop = structure.properties.emplace_back();
					prop.name = p.name;
					prop.type = std::move(p.type);
					prop.isOptional = p.isOptional;
					prop.documentation = p.documentation;
				}

				generate(structure);

				// HACK: Transfer unique_ptr ownership back from the temporary structure
				for(std::size_t i = 0; i < structure.properties.size(); ++i)
					structureLiteral.properties[i].type = std::move(structure.properties[i].type);

				break;
			}
		default:
			break;
		}
	}

	void generateStructureProperties(const std::vector<Structure::Property>& properties, std::string& toJson, std::string& fromJson, std::vector<std::string>& requiredProperties){
		for(const auto& p : properties){
			std::string typeName = cppTypeName(p.type, p.isOptional);

			m_typesHeaderFileContent += documentationComment({}, p.documentation, 1) +
			                            '\t' + typeName + ' ' + p.name + ";\n";
			if(p.isOptional){
				toJson += "\tif(value." + p.name + ")\n\t";
				fromJson += "\tif(json.contains(\"" + p.name + "\"))\n\t";
			}else{
				requiredProperties.push_back(p.name);
			}

			toJson += "\tjson[\"" + p.name + "\"] = toJson(value." + p.name + ");\n";
			fromJson += "\tfromJson(json.get(\"" + p.name + "\"), value." + p.name + ");\n";
		}
	}

	void generate(const Structure& structure){
		// Make sure dependencies are generated first
		{
			for(const auto& e : structure.extends)
				generateType(e, {});

			// Mixins technically don't have to be generated since their properties are directly copied into this structure.
			// However, generating them all here makes sure that all property types are also generated.
			for(const auto& m : structure.mixins)
				generateType(m, {});

			for(const auto& p : structure.properties)
				generateType(p.type, structure.name + util::str::capitalize(p.name));
		}

		m_typesHeaderFileContent += documentationComment(structure.name, structure.documentation) +
		                            "struct " + structure.name;

		std::string propertiesToJson = "static void " + util::str::uncapitalize(structure.name) + "ToJson("
		                               "const " + structure.name + "& value, json::Object& json){\n";
		std::string propertiesFromJson = "static void " + util::str::uncapitalize(structure.name) + "FromJson("
		                                 "const json::Object& json, " + structure.name + "& value){\n";
		std::string requiredPropertiesSig = "template<>\nconst char** requiredProperties<" + structure.name + ">()";
		std::vector<std::string> requiredPropertiesList;
		std::string requiredProperties = requiredPropertiesSig + "{\n\tstatic const char* properties[] = {\n";

		// Add base classes

		if(auto it = structure.extends.begin(); it != structure.extends.end()){
			const auto* extends = &(*it)->as<ReferenceType>();
			m_typesHeaderFileContent += " : " + extends->name;
			std::string lower = util::str::uncapitalize(extends->name);
			propertiesToJson += '\t' + lower + "ToJson(value, json);\n";
			propertiesFromJson += '\t' + lower + "FromJson(json, value);\n";
			++it;

			while(it != structure.extends.end()){
				extends = &(*it)->as<ReferenceType>();
				m_typesHeaderFileContent += ", " + extends->name;
				lower = util::str::uncapitalize(extends->name);
				propertiesToJson += '\t' + lower + "ToJson(value, json);\n";
				propertiesFromJson += '\t' + lower + "FromJson(json, value);\n";
				++it;
			}
		}

		m_typesHeaderFileContent += "{\n";

		// Generate properties

		for(const auto& m : structure.mixins){
			if(!m->isA<ReferenceType>())
				throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a type reference"};

			auto type = m_metaModel.typeForName(m->as<ReferenceType>().name);

			if(!std::holds_alternative<const Structure*>(type))
				throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a structure type"};

			generateStructureProperties(std::get<const Structure*>(type)->properties, propertiesToJson, propertiesFromJson, requiredPropertiesList);
		}

		generateStructureProperties(structure.properties, propertiesToJson, propertiesFromJson, requiredPropertiesList);

		propertiesToJson += "}\n\n";
		propertiesFromJson += "}\n\n";

		for(const auto& p : requiredPropertiesList)
			requiredProperties += "\t\t\"" + p + "\",\n";

		requiredProperties += "\t\tnullptr\n\t};\n\treturn properties;\n}\n\n";

		std::string toJson = toJsonSig(structure.name);
		std::string fromJson = fromJsonSig(structure.name);

		m_typesHeaderFileContent += "\n\tvoid initWithJson(const json::Object& json);\n"
		                            "};\n\n";

		if(!requiredPropertiesList.empty()){
			m_typesHeaderFileContentBoilerPlate += requiredPropertiesSig + ";\n";
			m_typesSourceFileContent += requiredProperties;
		}

		m_typesHeaderFileContentBoilerPlate += toJson + ";\n" +
		                                       fromJson + ";\n";
		m_typesSourceFileContent += propertiesToJson + propertiesFromJson +
		                            toJson + "{\n"
		                            "\tjson::Object obj;\n"
		                            "\t" + util::str::uncapitalize(structure.name) + "ToJson(value, obj);\n"
		                            "\treturn obj;\n"
		                            "}\n\n" +
		                            fromJson + "{\n"
		                            "\tconst auto& obj = json.get<json::Object>();\n"
		                            "\t" + util::str::uncapitalize(structure.name) + "FromJson(obj, value);\n"
		                            "}\n\n" +
		                            "void " + structure.name + "::initWithJson(const json::Object& json){\n"
		                            "\t" + util::str::uncapitalize(structure.name) + "FromJson(json, *this);\n"
		                            "}\n\n";
	}

	void generate(const TypeAlias& typeAlias){
		generateType(typeAlias.type, typeAlias.name + "Impl");

		m_typesHeaderFileContent += documentationComment(typeAlias.name, typeAlias.documentation) +
		                            "using " + typeAlias.name + " = " + cppTypeName(typeAlias.type) + ";\n\n";
	}
};

const CppGenerator::CppBaseType CppGenerator::s_stringBaseType{
	"json::String",
	"std::string_view",
	"const json::String&",
	"const json::String&"
};

const CppGenerator::CppBaseType CppGenerator::s_baseTypeMapping[] = {
	[BaseType::Boolean]     = {"bool", "bool", "bool", "bool"},
	[BaseType::String]      = s_stringBaseType,
	[BaseType::Integer]     = {"json::Integer", "json::Integer", "json::Integer", "json::Integer"},
	[BaseType::UInteger]    = {"json::Integer", "json::Integer", "json::Integer", "json::Integer"},
	[BaseType::Decimal]     = {"json::Decimal", "json::Decimal", "json::Decimal", "json::Decimal"},
	[BaseType::URI]         = s_stringBaseType,
	[BaseType::DocumentUri] = s_stringBaseType,
	[BaseType::RegExp]      = s_stringBaseType,
	[BaseType::Null]        = {"json::Null", "json::Null", "json::Null", "json::Null"}
};

int main(int argc, char** argv){
	if(argc != 2){
		std::cerr << "Expected the input file name as the first and only argument" << std::endl;
		return EXIT_FAILURE;
	}

	int ExitCode = EXIT_SUCCESS;
	const char* inputFileName = argv[1];

	if(std::ifstream in{inputFileName, std::ios::binary}){
		try{
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
			generator.generateTypes();
			generator.writeFiles();
		}catch(const json::ParseError& e){
			std::cerr << "JSON parse error at offset " << e.textPos() << ": " << e.what() << std::endl;
			ExitCode = EXIT_FAILURE;
		}catch(const std::exception& e){
			std::cerr << "Error: " << e.what() << std::endl;
			ExitCode = EXIT_FAILURE;
		}catch(...){
			std::cerr << "Unknown error" << std::endl;
			ExitCode = EXIT_FAILURE;
		}
	}else{
		std::cerr << "Failed to open " << inputFileName << std::endl;
		ExitCode = EXIT_FAILURE;
	}

	return ExitCode;
}
