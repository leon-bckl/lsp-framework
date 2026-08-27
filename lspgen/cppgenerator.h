#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include "metamodel.h"

namespace lspgen{

class CppGenerator{
public:
	CppGenerator(const MetaModel* metaModel) : m_metaModel{*metaModel}{}

	void generate();
	void writeFiles();

private:
	std::string                                  m_typesHeaderFileContent;
	std::string                                  m_typesBoilerPlateHeaderFileContent;
	std::string                                  m_typesBoilerPlateSourceFileContent;
	std::string                                  m_typesSourceFileContent;
	std::string                                  m_messagesHeaderFileContent;
	const MetaModel&                             m_metaModel;
	std::unordered_set<std::string_view>         m_processedTypes;
	std::unordered_set<std::string_view>         m_typesBeingProcessed;
	std::unordered_map<const Type*, std::string> m_generatedTypeNames;

	struct CppBaseType{
		std::string name;
	};

	static const CppBaseType s_baseTypeMapping[BaseType::MAX];

	void generateTypes();
	void generateMessages();
	void generateMessage(const std::string& method, const Message& message, bool isNotification);

	static void writeFile(const std::string& name, std::string_view content);
	static std::string upperCaseIdentifier(std::string_view str);
	static std::string writeJsonSig(const std::string& typeName);
	static std::string fromJsonSig(const std::string& typeName);
	static std::string documentationComment(const std::string& title, const std::string& documentation, std::size_t indentLevel = 0);

	void generateNamedType(std::string_view name);
	void generate(const Enumeration& enumeration);
	bool isStringType(const TypePtr& type);
	std::string cppTypeName(const Type& type, bool optional = false);
	void generateAggregateTypeList(const std::vector<TypePtr>& typeList, const std::string& baseName);
	void generateType(const TypePtr& type, const std::string& baseName, bool alias = false);

	void generateStructureProperties(const std::vector<StructureProperty>& properties,
	                                 const std::unordered_map<std::string_view,
	                                 const StructureProperty*>& basePropertiesByName,
	                                 std::string& writeJson,
	                                 std::string& fromJson,
	                                 std::vector<std::string>& requiredProperties,
	                                 std::vector<std::pair<std::string, std::string>>& literalProperties,
	                                 std::vector<std::pair<std::string_view, std::string>>& inheritedLiterals);

	void generate(const Structure& structure);
	void generate(const TypeAlias& typeAlias);
};

} // namespace lspgen
