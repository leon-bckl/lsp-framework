#include <algorithm>
#include <cassert>
#include <format>
#include <stdexcept>
#include <variant>
#include "metamodel.h"
#include "cpptypegenerator.h"
#include "util.h"

namespace lspgen{
namespace{

static auto baseTypeName(BaseType::Kind kind) -> std::string_view
{
	switch(kind)
	{
		using enum BaseType::Kind;
	case Boolean:
		return "bool";
	case String:
		return "String";
	case Integer:
		return "int";
	case UInteger:
		return "uint";
	case Decimal:
		return "double";
	case URI:
		return "Uri";
	case DocumentUri:
		return "DocumentUri";
	case RegExp:
		return "String";
	case Null:
		return "Null";
	case MAX:
		break;
	}

	throw std::logic_error("Invalid BaseType::Kind");
}

static auto literalPropertyValue(const StructureProperty& property) -> std::string
{
	switch(property.type->category())
	{
	case Type::StringLiteral:
		return json::stringify(std::string(property.type->as<StringLiteralType>().stringValue));
	case Type::IntegerLiteral:
		return std::to_string(property.type->as<IntegerLiteralType>().integerValue);
	case Type::BooleanLiteral:
		return std::string{property.type->as<BooleanLiteralType>().booleanValue ? "true" : "false"};
	default:
		break;
	}

	return {};
}

} // namespace

void CppTypeGenerator::generate(const MetaModel& metaModel)
{
	m_metaModel           = &metaModel;
	m_processedTypes      = {"LSPArray", "LSPObject", "LSPAny"};
	m_typesBeingProcessed = {};
	m_typeWriter.reset();
	m_declWriter.reset();
	m_implWriter.reset();

	m_declWriter.writeDocComment("Serialization boilerplate", {});
	m_declWriter.writeEmptyLine();

	for(const auto& name : m_metaModel->typeNames())
		generateNamedType(name);
}

void CppTypeGenerator::generateNamedType(std::string_view name)
{
	if(m_processedTypes.contains(name))
		return;

	m_processedTypes.insert(name);
	std::visit([this](const auto* ptr)
		{
			m_typesBeingProcessed.insert(ptr->name);
			generate(*ptr);
			m_typesBeingProcessed.erase(ptr->name);
		}, m_metaModel->typeForName(name));
}

void CppTypeGenerator::generate(const Enumeration& enumeration)
{
	if(!enumeration.type->isA<BaseType>())
		throw std::runtime_error("Enumeration value type for '" + enumeration.name + "' must be a base type");

	const auto enumTypeCppName = CppWriter::upperCaseIdentifier(enumeration.name);
	const auto baseType        = baseTypeName(enumeration.type->as<BaseType>().kind);

	m_typeWriter.writeDocComment(enumeration.name, enumeration.documentation);
	m_typeWriter.writeEnumStart(enumTypeCppName);

	for(const auto& value : enumeration.values)
	{
		m_typeWriter.writeDocComment({}, value.documentation);
		m_typeWriter.writeLine(CppWriter::upperCaseIdentifier(value.name) + ',');
	}

	m_typeWriter.writeLine("MAX_VALUE");
	m_typeWriter.writeEnumEnd();

	const auto enumCppTemplateType = std::format("Enumeration<{}, {}>", enumTypeCppName, baseType);
	const auto enumCppName         = enumTypeCppName + "Enum";
	m_typeWriter.writeTypedef(enumCppName, enumCppTemplateType);
	m_typeWriter.writeEmptyLine();

	m_declWriter.writeLine("template<>");
	m_declWriter.writeLine(std::format("const {0}::ConstInitType {0}::s_values[];", enumCppName));
}

void CppTypeGenerator::generate(const Structure& structure)
{
	const auto structCppName = CppWriter::upperCaseIdentifier(structure.name);

	// Make sure dependencies are generated first
	{
		for(const auto& e : structure.extends)
			generateType(e);

		// Mixins technically don't have to be generated since their properties are directly copied into this structure.
		// However, generating them all here makes sure that all property types are also generated.
		for(const auto& m : structure.mixins)
			generateType(m);

		// Generate types for struct properties so that inline struct literals get their own named type
		for(const auto& p : structure.properties)
			generateType(p.type, structCppName + CppWriter::upperCaseIdentifier(p.name));
	}

	auto extendsList = std::vector<std::string>();

	for(const auto& ext : structure.extends)
		extendsList.push_back(CppWriter::upperCaseIdentifier(ext->as<ReferenceType>().name));

	m_typeWriter.writeDocComment(structure.name, structure.documentation);
	m_typeWriter.writeStructStart(structCppName, joinStrings(extendsList, ", "));

	auto       inheritedLiterals     = std::vector<const StructureProperty*>();
	const auto checkInheritedLiteral = [&](const StructureProperty& p)
		{
			if(p.type->isLiteral() && !!structure.findBaseProperty(p.name, *m_metaModel))
				inheritedLiterals.push_back(&p);
		};

	const auto writeJsonParams = CppWriter::FuncParamList{
		{CppWriter::type(structCppName, CppWriter::TypeConst | CppWriter::TypeRef), "value"},
		{CppWriter::type("json::ObjectWriter", CppWriter::TypeRef), "objectWriter"}};

	m_declWriter.writeFuncSig("writeJson", "void", writeJsonParams);
	m_declWriter.writeLine(";");

	m_implWriter.writeFuncSig("writeJson", "void", writeJsonParams);
	m_implWriter.writeBlockStart(true);

	for(const auto& ext : extendsList)
		m_implWriter.writeLine("writeJson(static_cast<const " + ext + "&>(value), objectWriter);");

	for(const auto& mixin : structure.mixins)
	{
		const auto& mixinTypeName = mixin->as<ReferenceType>().name;
		const auto& structType    = m_metaModel->typeForName(mixinTypeName);
		const auto  structure     = std::get<const Structure*>(structType);

		for(const auto& property : structure->properties)
		{
			generateStructureProperty(*structure, property);
			checkInheritedLiteral(property);
		}
	}

	for(const auto& property : structure.properties)
	{
		generateStructureProperty(structure, property);
		checkInheritedLiteral(property);
	}

	// Write constructor to initialize inherited literal properties
	if(!inheritedLiterals.empty())
	{
		m_typeWriter.writeEmptyLine();
		m_typeWriter.writeFuncSig(structCppName, {}, {});
		m_typeWriter.writeBlockStart(true);

		for(const auto* p : inheritedLiterals)
		{
			m_typeWriter.write(p->name);
			m_typeWriter.write(" = ");
			m_typeWriter.write(literalPropertyValue(*p));
			m_typeWriter.writeLine(";");
		}

		m_typeWriter.writeBlockEnd(false, false);
	}

	m_typeWriter.writeStructEnd();
	m_implWriter.writeBlockEnd(false, true);
}

void CppTypeGenerator::generate(const TypeAlias& typeAlias)
{
	const auto typeAliasCppName = CppWriter::upperCaseIdentifier(typeAlias.name);
	generateType(typeAlias.type, typeAliasCppName, true);
	m_typeWriter.writeDocComment(typeAlias.name, typeAlias.documentation);
	m_typeWriter.writeTypedef(typeAliasCppName, cppTypeName(*typeAlias.type));
	m_typeWriter.writeEmptyLine();
}

void CppTypeGenerator::generateType(const TypePtr& type, const std::string& baseName, bool alias)
{
	switch(type->category())
	{
	case Type::Base:
		break;
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
	case Type::StringLiteral:
	case Type::IntegerLiteral:
	case Type::BooleanLiteral:
		break;
	}
}

void CppTypeGenerator::generateAggregateTypeList(const std::vector<TypePtr>& typeList, const std::string& baseName)
{
	for(const auto& t : typeList)
	{
		// Make unique type name for struct literal by including all non-optional properties in the name
		auto nameSuffix = std::string();

		if(t->isA<StructureLiteralType>())
		{
			const auto& structLit = t->as<StructureLiteralType>();

			for(const auto& p : structLit.properties)
			{
				if(!p.isOptional)
					nameSuffix += '_' + CppWriter::upperCaseIdentifier(p.name);
			}
		}

		generateType(t, baseName + nameSuffix);
	}
}

void CppTypeGenerator::generateStructureProperty(const Structure& structure, const StructureProperty& property)
{
	const auto isLiteral          = property.type->isLiteral();
	const auto isInheritedLiteral = isLiteral && !!structure.findBaseProperty(property.name, *m_metaModel);

	// Don't write literal properties with the same name as an inherited property.
	// Instead initialize the inherited property in the constructor later.
	if(!isInheritedLiteral)
	{
		m_typeWriter.writeDocComment({}, property.documentation);

		auto initializer = std::string();

		if(isLiteral)
			initializer = literalPropertyValue(property);
		else if(property.isOptional)
			initializer = "{}"; // Explicitly initialize optionals to avoid warnings with designated initializers that omit them

		m_typeWriter.writeVariable(property.name, cppTypeName(*property.type, property.isOptional), initializer);

		if(property.isOptional)
		{
			m_implWriter.writeLine("if(value." + property.name + ")");
			m_implWriter.indent();
		}

		m_implWriter.writeLine("writeJson(\"" + property.name + "\", value." + property.name + ", objectWriter);");

		if(property.isOptional)
			m_implWriter.outdent();
	}
}

auto CppTypeGenerator::cppTypeName(const Type& type, bool optional) -> std::string
{
	std::string typeName;

	if(optional)
	{
		if(!type.isA<ReferenceType>() || !m_typesBeingProcessed.contains(type.as<ReferenceType>().name))
			typeName = "Opt<";
		else
			typeName = "Ptr<";
	}

	switch(type.category())
	{
	case Type::Base:
		typeName += baseTypeName(type.as<BaseType>().kind);
		break;
	case Type::Reference:
		{
			const auto& ref = type.as<ReferenceType>();
			typeName += CppWriter::upperCaseIdentifier(ref.name);

			const auto typeVariant = m_metaModel->typeForName(ref.name);
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

			typeName += "Map<" + cppTypeName(*keyType) + ", " + cppTypeName(*valueType) + '>';
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
				const auto nullType = std::ranges::find_if(orType.typeList,
					[](const TypePtr& type)
					{
						return type->isA<BaseType>() && type->as<BaseType>().kind == BaseType::Null;
					});
				auto cppOrType = std::string();

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
} // namespace lspgen
