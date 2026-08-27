#include <algorithm>
#include <cassert>
#include <fstream>
#include "cppgenerator.h"
#include "util.h"

namespace lspgen{

static constexpr const char* TypesHeaderBegin =
R"(#pragma once

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include <string>
#include <tuple>
#include <variant>
#include <vector>
#include <unordered_map>
#include <lsp/enumeration.h>
#include <lsp/json/json.h>
#include <lsp/nullable.h>
#include <lsp/serialization.h>
#include <lsp/uri.h>
#include <lsp/version.h>

namespace lsp{

#define LSP_PROTOCOL_VERSION_MAJOR ${LSP_PROTOCOL_VERSION_MAJOR}
#define LSP_PROTOCOL_VERSION_MINOR ${LSP_PROTOCOL_VERSION_MINOR}
#define LSP_PROTOCOL_VERSION_PATCH ${LSP_PROTOCOL_VERSION_PATCH}
#define LSP_PROTOCOL_VERSION LSP_INT_VERSION(LSP_PROTOCOL_VERSION_MAJOR, LSP_PROTOCOL_VERSION_MINOR, LSP_PROTOCOL_VERSION_PATCH)
#define LSP_PROTOCOL_VERSION_STR LSP_STRINGIFY_VERSION(LSP_PROTOCOL_VERSION_MAJOR, LSP_PROTOCOL_VERSION_MINOR, LSP_PROTOCOL_VERSION_PATCH)

using Null        = std::nullptr_t;
using uint        = unsigned int;
using String      = std::string;
using DocumentUri = Uri;
using LSPArray    = json::Array;
using LSPObject   = json::Object;
using LSPAny      = json::Value;

template<typename T>
using Opt = std::optional<T>;

template<typename... Args>
using Tuple = std::tuple<Args...>;

template<typename... Args>
using OneOf = std::variant<Args...>;

template<typename T>
using NullOr = Nullable<T>;

template<typename... Args>
using NullOrOneOf = NullableVariant<Args...>;

template<typename T>
using Array = std::vector<T>;

template<typename K, typename T>
using Map = std::unordered_map<K, T>;

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

namespace lsp{

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter
#endif

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

#include <lsp/messagebase.h>
#include "types.h"

namespace lsp{

)";

static constexpr const char* MessagesHeaderEnd =
R"(} // namespace lsp
)";

void CppGenerator::generate()
{
	generateTypes();
	generateMessages();
}

void CppGenerator::writeFiles()
{
	const auto versionStr   = m_metaModel.metaData().version;
	const auto versionParts = splitStringView(versionStr, ".");

	if(versionParts.size() != 3)
		throw std::runtime_error("Could not determine version parts from '" + std::string(versionStr) + "'");

	auto typesHeader = replaceString(TypesHeaderBegin, "${LSP_PROTOCOL_VERSION_MAJOR}", versionParts[0]);
	typesHeader      = replaceString(typesHeader,      "${LSP_PROTOCOL_VERSION_MINOR}", versionParts[1]);
	typesHeader      = replaceString(typesHeader,      "${LSP_PROTOCOL_VERSION_PATCH}", versionParts[2]);

	writeFile("types.h", typesHeader + m_typesHeaderFileContent + m_typesBoilerPlateHeaderFileContent + TypesHeaderEnd);
	writeFile("types.cpp", TypesSourceBegin + m_typesSourceFileContent + m_typesBoilerPlateSourceFileContent + TypesSourceEnd);
	writeFile("messages.h", MessagesHeaderBegin + m_messagesHeaderFileContent + MessagesHeaderEnd);
}

void CppGenerator::generateTypes()
{
	m_processedTypes = {"LSPArray", "LSPObject", "LSPAny"};
	m_typesBeingProcessed = {};
	m_typesBoilerPlateHeaderFileContent = "/*\n * Serialization boilerplate\n */\n\n";
	m_typesBoilerPlateSourceFileContent = m_typesBoilerPlateHeaderFileContent;

	for(const auto& name : m_metaModel.typeNames())
		generateNamedType(name);
}

void CppGenerator::generateMessages()
{
	const char* namespaceStr = "/*\n"
														 " * Request messages\n"
														 " */\n"
														 "namespace requests{\n\n";

	m_messagesHeaderFileContent += namespaceStr;

	for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Request))
		generateMessage(method, message, false);

	namespaceStr = "} // namespace requests\n\n"
								 "/*\n"
								 " * Notification messages\n"
								 " */\n"
								 "namespace notifications{\n\n";


	m_messagesHeaderFileContent += namespaceStr;

	for(const auto& [method, message] : m_metaModel.messagesByName(MetaModel::MessageType::Notification))
		generateMessage(method, message, true);

	namespaceStr = "} // namespace notifications\n";

	m_messagesHeaderFileContent += namespaceStr;
}

void CppGenerator::generateMessage(const std::string& method, const Message& message, bool isNotification)
{
	auto messageCppName = upperCaseIdentifier(method);
	std::string messageDirection;

	switch(message.direction)
	{
	case Message::Direction::ClientToServer:
		messageDirection = "ClientToServer";
		break;
	case Message::Direction::ServerToClient:
		messageDirection = "ServerToClient";
		break;
	case Message::Direction::Both:
		messageDirection = "Bidirectional";
	}

	m_messagesHeaderFileContent += documentationComment(method, message.documentation) +
																 "struct " + messageCppName + "{\n"
																 "\tstatic constexpr auto Method    = std::string_view(\"" + method + "\");\n"
																 "\tstatic constexpr auto Direction = MessageDirection::" + messageDirection + ";\n"
																 "\tstatic constexpr auto Type      = Message::" + (isNotification ? "Notification" : "Request") + ";\n";

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

void CppGenerator::writeFile(const std::string& name, std::string_view content)
{
	std::ofstream file{name, std::ios::trunc | std::ios::binary};
	file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string CppGenerator::upperCaseIdentifier(std::string_view str)
{
	if(str.starts_with('$'))
		str.remove_prefix(1);

	auto parts = splitStringView(str, "/", true);
	auto id = joinStrings(parts, "_", [](auto&& s){ return capitalizeString(s); });

	std::transform(id.cbegin(), id.cend(), id.begin(), [](char c)
	{
		if(!std::isalnum(c) && c != '_')
			return '_';

		return c;
	});

	return id;
}

std::string CppGenerator::writeJsonSig(const std::string& typeName)
{
	return "void writeJson(const " + typeName + "& value, json::ObjectWriter& objectWriter)";
}

std::string CppGenerator::fromJsonSig(const std::string& typeName)
{
	return "void fromJson(json::Value&& json, " + typeName + "& value)";
}

std::string CppGenerator::documentationComment(const std::string& title, const std::string& documentation, std::size_t indentLevel)
{
	std::string indent(indentLevel, '\t');
	std::string comment = indent + "/*\n";

	if(!title.empty())
		comment += indent + " * " + title + "\n";

	auto documentationLines = splitStringView(documentation, "\n");

	if(!documentationLines.empty())
	{
		if(!title.empty())
			comment += indent + " *\n";

		for(const auto& line : documentationLines)
			comment += indent + " * " + replaceString(replaceString(line, "/*", "/_*"), "*/", "*_/") + '\n';
	}
	else if(title.empty())
	{
	 return {};
	}

	comment += indent + " */\n";

	return comment;
}

void CppGenerator::generateNamedType(std::string_view name)
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

void CppGenerator::generate(const Enumeration& enumeration)
{
	if(!enumeration.type->isA<BaseType>())
		throw std::runtime_error{"Enumeration value type for '" + enumeration.name + "' must be a base type"};

	const auto enumTypeCppName = upperCaseIdentifier(enumeration.name);
	const auto enumerationCppName = enumTypeCppName + "Enum";

	const auto& baseType = s_baseTypeMapping[enumeration.type->as<BaseType>().kind];

	m_typesHeaderFileContent += documentationComment(enumTypeCppName, enumeration.documentation);

	m_typesHeaderFileContent += "enum class " + enumTypeCppName + "{\n";

	m_typesSourceFileContent += "template<>\n"
															"const " + enumerationCppName + "::ConstInitType " +
															enumerationCppName + "::s_values[] = {\n";

	if(auto it = enumeration.values.begin(); it != enumeration.values.end())
	{
		m_typesHeaderFileContent += documentationComment({}, it->documentation, 1) +
																'\t' + capitalizeString(it->name);
		m_typesSourceFileContent += '\t' + json::stringify(it->value);
		++it;

		while(it != enumeration.values.end())
		{
			m_typesHeaderFileContent += ",\n" + documentationComment({}, it->documentation, 1) + '\t' + capitalizeString(it->name);
			m_typesSourceFileContent += ",\n\t" + json::stringify(it->value);
			++it;
		}
	}

	const auto enumCppTemplateType = "Enumeration<" + enumTypeCppName + ", " + baseType.name + '>';

	m_typesHeaderFileContent += ",\n\tMAX_VALUE\n"
															"};\n"
															"using " + enumTypeCppName + "Enum = " + enumCppTemplateType + ";\n"
															"template<>\n"
															"const " + enumerationCppName + "::ConstInitType " + enumerationCppName + "::s_values[];\n\n";

	m_typesSourceFileContent += "\n};\n\n";
}

bool CppGenerator::isStringType(const TypePtr& type)
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

std::string CppGenerator::cppTypeName(const Type& type, bool optional)
{
	std::string typeName;

	if(optional)
	{
		if(!type.isA<ReferenceType>() || !m_typesBeingProcessed.contains(type.as<ReferenceType>().name))
			typeName = "Opt<";
		else
			typeName = "std::unique_ptr<";
	}

	switch(type.category())
	{
	case Type::Base:
		typeName += s_baseTypeMapping[static_cast<int>(type.as<BaseType>().kind)].name;
		break;
	case Type::Reference:
		{
			const auto& ref = type.as<ReferenceType>();
			typeName += upperCaseIdentifier(ref.name);

			const auto typeVariant = m_metaModel.typeForName(ref.name);
			if(std::holds_alternative<const Enumeration*>(typeVariant))
				typeName += "Enum";

			break;
		}
	case Type::Array:
		{
			const auto& arrayType = type.as<ArrayType>();
			if(arrayType.elementType->isA<ReferenceType>() && arrayType.elementType->as<ReferenceType>().name == "LSPAny")
				typeName += "LSPArray";
			else
				typeName += "Array<" + cppTypeName(*arrayType.elementType) + '>';

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
					cppOrType = "OneOf<";
				else if(orType.typeList.size() > 2)
					cppOrType = "NullOrOneOf<";
				else
					cppOrType = "NullOr<";

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
			std::string cppTupleType = "Tuple<";
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
		typeName += "String";
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

void CppGenerator::generateAggregateTypeList(const std::vector<TypePtr>& typeList, const std::string& baseName)
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
					nameSuffix += '_' + capitalizeString(p.name);
			}
		}

		generateType(t, baseName + nameSuffix);
	}
}

void CppGenerator::generateType(const TypePtr& type, const std::string& baseName, bool alias)
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

void CppGenerator::generateStructureProperties(const std::vector<StructureProperty>& properties,
                                               const std::unordered_map<std::string_view,
                                               const StructureProperty*>& basePropertiesByName,
                                               std::string& writeJson,
                                               std::string& fromJson,
                                               std::vector<std::string>& requiredProperties,
                                               std::vector<std::pair<std::string, std::string>>& literalProperties,
                                               std::vector<std::pair<std::string_view, std::string>>& inheritedLiterals)
{
	for(const auto& p : properties)
	{
		std::string literalValue;
		bool isInheritedLiteral = false;

		if(p.type->isLiteral())
		{
			switch(p.type->category())
			{
			case Type::StringLiteral:
				literalValue = json::stringify(std::string(p.type->as<StringLiteralType>().stringValue));
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
				inheritedLiterals.emplace_back(p.name, literalValue);
				isInheritedLiteral = true;
			}
		}

		// Don't write literal properties with the same name as an inherited property. Instead initialize the inherited property.
		if(!isInheritedLiteral)
		{
			const auto typeName = cppTypeName(*p.type, p.isOptional);

			m_typesHeaderFileContent += documentationComment({}, p.documentation, 1) +
																	'\t' + typeName + ' ' + p.name;

			if(!literalValue.empty())
				m_typesHeaderFileContent += " = " + literalValue;
			else if (p.isOptional)
				m_typesHeaderFileContent += " = {}";

			m_typesHeaderFileContent += ";\n";
		}

		if(p.isOptional)
		{
			writeJson += "\tif(value." + p.name + ")\n\t";
			fromJson += "\tif(auto* const v = json.find(\"" + p.name + "\"))\n\t"
									"\tfromJson(std::move(*v), value." + p.name + ");\n";
		}
		else
		{
			if(!isInheritedLiteral)
				fromJson += "\tfromJson(std::move(json.get(\"" + p.name + "\")), value." + p.name + ");\n";

			if(literalValue.empty())
				requiredProperties.push_back(p.name);
		}

		if(!literalValue.empty())
		{
			literalProperties.push_back({p.name, literalValue});
			fromJson += "\tif(value." + p.name + " != " + literalValue + ")\n"
									"\t\tthrow json::TypeError(\"Unexpected value for literal '" + p.name + "'\");\n";
		}

		if(!isInheritedLiteral)
			writeJson += "\twriteJson(\"" + p.name + "\", value." + p.name + ", objectWriter);\n";
	}
}

void CppGenerator::generate(const Structure& structure)
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
			generateType(p.type, structureCppName + capitalizeString(p.name));
	}

	m_typesHeaderFileContent += documentationComment(structureCppName, structure.documentation) +
															"struct " + structureCppName;

	std::string writeJson = writeJsonSig(structureCppName) + "\n{\n";
	std::string propertiesFromJson = "static void " + uncapitalizeString(structureCppName) + "FromJson("
																	 "json::Object& json, " + structureCppName + "& value)\n{\n";
	const std::string requiredPropertiesSig = "template<>\nconst char** requiredProperties<" + structureCppName + ">()";
	const std::string literalPropertiesSig = "template<>\nconst std::pair<const char*, json::Value>* literalProperties<" + structureCppName + ">()";
	std::vector<std::string>                         requiredPropertiesList;
	std::vector<std::pair<std::string, std::string>> literalPropertiesList;
	std::string requiredProperties = requiredPropertiesSig + "\n{\n\tstatic const char* properties[] = {\n";
	std::string literalProperties = literalPropertiesSig + "\n{\n\tstatic const std::pair<const char*, json::Value> properties[] = {\n";

	// Add base classes

	std::unordered_map<std::string_view, const StructureProperty*> basePropertiesByName;
	if(auto it = structure.extends.begin(); it != structure.extends.end())
	{
		const auto* extends = &(*it)->as<ReferenceType>();
		m_typesHeaderFileContent += " : " + extends->name;
		writeJson += "\twriteJson(static_cast<const " + extends->name + "&>(value), objectWriter);\n";
		propertiesFromJson += '\t' + uncapitalizeString(extends->name) + "FromJson(json, value);\n";
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
			writeJson += "\twriteJson(static_cast<const " + extends->name + "&>(value), objectWriter);\n";
			propertiesFromJson += '\t' + uncapitalizeString(extends->name) + "FromJson(json, value);\n";
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

	std::vector<std::pair<std::string_view, std::string>> inheritedLiterals;
	for(const auto& m : structure.mixins)
	{
		if(!m->isA<ReferenceType>())
			throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a type reference"};

		auto type = m_metaModel.typeForName(m->as<ReferenceType>().name);

		if(!std::holds_alternative<const Structure*>(type))
			throw std::runtime_error{"Mixin type for '" + structure.name + "' must be a structure type"};

		generateStructureProperties(std::get<const Structure*>(type)->properties, basePropertiesByName, writeJson, propertiesFromJson, requiredPropertiesList, literalPropertiesList, inheritedLiterals);
	}

	generateStructureProperties(structure.properties, basePropertiesByName, writeJson, propertiesFromJson, requiredPropertiesList, literalPropertiesList, inheritedLiterals);

	if(!inheritedLiterals.empty())
	{
		m_typesHeaderFileContent += "\n\t" + structure.name + "()\n\t{\n";

		for(const auto& v : inheritedLiterals)
		{
			m_typesHeaderFileContent += "\t\t";
			m_typesHeaderFileContent += v.first;
			m_typesHeaderFileContent += " = " + v.second + ";\n";
		}

		m_typesHeaderFileContent += "\t}\n";
	}

	m_typesHeaderFileContent += "};\n\n";

	writeJson += "}\n";
	propertiesFromJson += "}\n\n";

	for(const auto& p : requiredPropertiesList)
		requiredProperties += "\t\t\"" + p + "\",\n";

	requiredProperties += "\t\tnullptr\n\t};\n\treturn properties;\n}\n\n";

	for(const auto& p : literalPropertiesList)
		literalProperties += "\t\t{\"" + p.first + "\", " + p.second + "},\n";

	literalProperties += "\t\t{nullptr, {}}\n\t};\n\treturn properties;\n}\n\n";

	std::string fromJson = fromJsonSig(structureCppName);

	if(!requiredPropertiesList.empty())
	{
		m_typesBoilerPlateHeaderFileContent += requiredPropertiesSig + ";\n";
		m_typesBoilerPlateSourceFileContent += requiredProperties;
	}

	if(!literalPropertiesList.empty())
	{
		m_typesBoilerPlateHeaderFileContent += literalPropertiesSig + ";\n";
		m_typesBoilerPlateSourceFileContent += literalProperties;
	}

	m_typesBoilerPlateHeaderFileContent += writeJsonSig(structureCppName) + ";\n" +
																				 fromJson + ";\n";
	m_typesSourceFileContent += propertiesFromJson;
	m_typesBoilerPlateSourceFileContent += writeJson + "\n" +
																				 fromJson + "\n"
																				 "{\n"
																				 "\tauto& obj = json.object();\n"
																				 "\t" + uncapitalizeString(structureCppName) + "FromJson(obj, value);\n"
																				 "}\n\n";
}

void CppGenerator::generate(const TypeAlias& typeAlias)
{
	auto typeAliasCppName = upperCaseIdentifier(typeAlias.name);

	generateType(typeAlias.type, typeAliasCppName, true);

	m_typesHeaderFileContent += documentationComment(typeAliasCppName, typeAlias.documentation) +
															"using " + typeAliasCppName + " = " + cppTypeName(*typeAlias.type) + ";\n\n";
}

const CppGenerator::CppBaseType CppGenerator::s_baseTypeMapping[] = {
	{"bool"},
	{"String"},
	{"int"},
	{"uint"},
	{"double"},
	{"Uri"},
	{"DocumentUri"},
	{"String"},
	{"Null"}
};

} // namespace lspgen
