#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cppwriter.h"

#include <fstream>

namespace lspgen{

struct Type;
struct Enumeration;
struct Structure;
struct StructureProperty;
struct TypeAlias;
class MetaModel;
using TypePtr = std::unique_ptr<Type>;

class CppTypeGenerator{
public:
	void generate(const MetaModel& metaModel);
	auto headerText() const -> std::string;
	auto sourceText() const -> std::string;

	void writeFiles() const
	{
		auto file = std::ofstream("types.h", std::ios::trunc | std::ios::binary);
		auto text = headerText();
		file.write(text.data(), static_cast<std::streamsize>(text.size()));

		file = std::ofstream("types.cpp", std::ios::trunc | std::ios::binary);
		text = sourceText();
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
	}

private:
	const MetaModel*                             m_metaModel;
	std::unordered_set<std::string_view>         m_processedTypes;
	std::unordered_set<std::string_view>         m_typesBeingProcessed;
	std::unordered_map<const Type*, std::string> m_generatedTypeNames;
	CppWriter                                    m_typeWriter;
	CppWriter                                    m_declWriter;
	CppWriter                                    m_deserializationWriter;
	CppWriter                                    m_serializationWriter;

	void generateNamedType(std::string_view name);
	void generate(const Enumeration& enumeration);
	void generate(const Structure& structure);
	void generate(const TypeAlias& typeAlias);
	void generateType(const TypePtr& type, const std::string& baseName = {}, bool alias = false);
	void generateAggregateTypeList(const std::vector<TypePtr>& typeList, const std::string& baseName);
	auto collectRequiredProperties(const Structure& structure) -> std::vector<std::string>;
	auto collectLiteralProperties(const Structure& structure) -> std::vector<std::pair<std::string, std::string>>;
	void generateStructureProperty(const Structure& structure, const StructureProperty& p);

	auto cppTypeName(const Type& type, bool optional = false) -> std::string;
};

} // namespace lspgen
