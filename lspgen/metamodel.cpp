#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>
#include "metamodel.h"
#include "util.h"

namespace lspgen{
namespace{

auto extractDocumentation(const json::Object& json) -> std::string
{
	if(const auto* doc = json.find("documentation"))
		return doc->string();

	return {};
}

} // namespace

/*
 * Type
 */

auto Type::isLiteral() const -> bool
{
	const auto cat = category();
	return cat == StructureLiteral || cat == StringLiteral || cat == IntegerLiteral || cat == BooleanLiteral;
}

auto Type::categoryFromString(std::string_view str) -> Category
{
	for(std::size_t i = 0; i < std::size(TypeCategoryStrings); ++i)
	{
		if(TypeCategoryStrings[i] == str)
			return static_cast<Type::Category>(i);
	}

	throw std::runtime_error('\'' + std::string(str) + "' is not a valid type kind");
}

auto Type::createFromJson(const json::Object& json) -> TypePtr
{
	TypePtr result;
	auto category = categoryFromString(json.get("kind").string());

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
		throw std::logic_error("Invalid type category");
	}

	assert(result->category() == category);
	result->extract(json);

	return result;
}

/*
 * BaseType
 */

void BaseType::extract(const json::Object& json)
{
	kind = kindFromString(json.get("name").string());
}

auto BaseType::kindFromString(std::string_view str) -> Kind
{
	for(std::size_t i = 0; i < std::size(BaseTypeStrings); ++i)
	{
		if(BaseTypeStrings[i] == str)
			return static_cast<Kind>(i);
	}

	throw std::runtime_error('\'' + std::string(str) + "' is not a valid base type");
}

/*
 * ReferenceType
 */

void ReferenceType::extract(const json::Object& json)
{
	name = json.get("name").string();
}

/*
 * ArrayType
 */

void ArrayType::extract(const json::Object& json)
{
	const auto& elementTypeJson = json.get("element").object();
	elementType = createFromJson(elementTypeJson);
}

/*
 * MapType
 */

void MapType::extract(const json::Object& json)
{
	keyType = createFromJson(json.get("key").object());
	valueType = createFromJson(json.get("value").object());
}

/*
 * AndType
 */

void AndType::extract(const json::Object& json)
{
	const auto& items = json.get("items").array();
	typeList.reserve(items.size());

	for(const auto& i : items)
		typeList.push_back(createFromJson(i.object()));
}

/*
 * OrType
 */

void OrType::extract(const json::Object& json)
{
	const auto& items = json.get("items").array();
	typeList.reserve(items.size());

	auto structureLiterals = std::vector<std::unique_ptr<Type>>();

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
		throw std::runtime_error("OrType must not be empty!");
}

/*
 * TupleType
 */

void TupleType::extract(const json::Object& json)
{
	const auto& items = json.get("items").array();
	typeList.reserve(items.size());

	for(const auto& i : items)
		typeList.push_back(createFromJson(i.object()));
}

/*
 * StructureProperty
 */

void StructureProperty::extract(const json::Object& json)
{
	name = json.get("name").string();
	type = Type::createFromJson(json.get("type").object());

	if(const auto* opt = json.find("optional"))
		isOptional = opt->boolean();

	documentation = extractDocumentation(json);
}

auto extractStructureProperties(const json::Array& json) -> StructurePropertyList
{
	StructurePropertyList result;
	result.reserve(json.size());
	std::transform(
		json.begin(),
		json.end(),
		std::back_inserter(result),
		[](const json::Value& e)
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

/*
 * StructureLiteralType
 */

void StructureLiteralType::extract(const json::Object& json)
{
	const auto& value = json.get("value").object();
	properties = extractStructureProperties(value.get("properties").array());
}

/*
 * StringLiteralType
 */

void StringLiteralType::extract(const json::Object& json)
{
	stringValue = json.get("value").string();
}

/*
 * IntegerLiteralType
 */

void IntegerLiteralType::extract(const json::Object& json)
{
	integerValue = static_cast<json::Integer>(json.get("value").number());
}

/*
 * BooleanLiteralType
 */

void BooleanLiteralType::extract(const json::Object& json)
{
	booleanValue = json.get("value").boolean();
}

/*
 * Enumeration
 */

void Enumeration::extract(const json::Object& json)
{
	name = json.get("name").string();
	const auto& typeJson = json.get("type").object();
	type = Type::createFromJson(typeJson);
	const auto& valuesJson = json.get("values").array();
	values.reserve(valuesJson.size());

	for(const auto& v : valuesJson)
	{
		const auto& obj = v.object();
		auto& enumValue = values.emplace_back();
		enumValue.name = obj.get("name").string();
		enumValue.value = obj.get("value");
		enumValue.documentation = extractDocumentation(obj);
	}

	documentation = extractDocumentation(json);

	if(const auto* supportsCustom = json.find("supportsCustomValues"))
		supportsCustomValues = supportsCustom->boolean();
}

/*
 * Structure
 */

void Structure::extract(const json::Object& json)
{
	name = json.get("name").string();
	properties = extractStructureProperties(json.get("properties").array());

	if(const auto* extListJson = json.find("extends"))
	{
		const auto& extList = extListJson->array();
		extends.reserve(extList.size());

		for(const auto& e : extList)
			extends.push_back(Type::createFromJson(e.object()));
	}

	if(const auto* mixinListJson = json.find("mixins"))
	{
		const auto& mixinList = mixinListJson->array();
		mixins.reserve(mixinList.size());

		for(const auto& e : mixinList)
			mixins.push_back(Type::createFromJson(e.object()));
	}

	documentation = extractDocumentation(json);
}

static auto findStructureProperty(const std::vector<TypePtr>& types, std::string_view name, const MetaModel& metaModel) -> const StructureProperty*
{
	for(const auto& type : types)
	{
		if(!type->isA<ReferenceType>())
			continue;

		const auto typeName   = static_cast<const ReferenceType&>(*type).name;
		const auto structType = metaModel.typeForName(typeName);

		if(const auto* structure = std::get_if<const Structure*>(&structType))
		{
			const auto* prop = (*structure)->findProperty(name, metaModel);

			if(prop)
				return prop;
		}
	}

	return nullptr;
}

auto Structure::findBaseProperty(std::string_view name, const MetaModel& metaModel) const -> const StructureProperty*
{
	return findStructureProperty(extends, name, metaModel);
}

auto Structure::findProperty(std::string_view name, const MetaModel& metaModel) const -> const StructureProperty*
{
	for(const auto& prop : properties)
	{
		if(prop.name == name)
			return &prop;
	}

	return findStructureProperty(mixins, name, metaModel);
}

/*
 * TypeAlias
 */

void TypeAlias::extract(const json::Object& json)
{
	name = json.get("name").string();
	type = Type::createFromJson(json.get("type").object());
	documentation = extractDocumentation(json);
}

/*
 * Message
 */

void Message::extract(const json::Object& json)
{
	documentation = extractDocumentation(json);

	if(const auto* clientCap = json.find("clientCapability"))
		clientCapabilityName = clientCap->string();

	if(const auto* serverCap = json.find("serverCapability"))
		serverCapabilityName = serverCap->string();

	const auto& dir = json.get("messageDirection").string();

	if(dir == "clientToServer")
		direction = Direction::ClientToServer;
	else if(dir == "serverToClient")
		direction = Direction::ServerToClient;
	else if(dir == "both")
		direction = Direction::Both;
	else
		throw std::runtime_error("Invalid message direction: " + dir);

	const auto memberTypeName = [](const json::Object& json, std::string_view key) -> std::string
	{
		if(const auto* typeJson = json.find(key))
		{
			const auto& typeObj = typeJson->object();

			if(typeObj.get("kind").string() == "reference")
				return typeObj.get("name").string();

			return json.get("method").string() + capitalizeString(key);
		}

		return {};
	};

	paramsTypeName              = memberTypeName(json, "params");
	resultTypeName              = memberTypeName(json, "result");
	partialResultTypeName       = memberTypeName(json, "partialResult");
	errorDataTypeName           = memberTypeName(json, "errorData");
	registrationOptionsTypeName = memberTypeName(json, "registrationOptions");
}

/*
 * MetaModel
 */

MetaModel::MetaModel() = default;

MetaModel::MetaModel(const json::Object& json)
{
	extract(json);
}

void MetaModel::extract(const json::Object& json)
{
	extractMetaData(json);
	extractTypes(json);
	extractMessages(json);
}

auto MetaModel::typeForName(std::string_view name) const -> TypeVariant
{
	if(auto it = m_typesByName.find(std::string(name)); it != m_typesByName.end())
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

	throw std::runtime_error("Type with name '" + std::string(name) + "' does not exist");
}

auto MetaModel::messagesByType(MessageType type) const -> const std::map<std::string, Message>&
{
	if(type == MessageType::Request)
		return m_requestsByMethod;

	assert(type == MessageType::Notification);
	return m_notificationsByMethod;
}

void MetaModel::extractMetaData(const json::Object& json)
{
	const auto& metaDataJson = json.get("metaData").object();
	m_metaData.version = metaDataJson.get("version").string();
}

void MetaModel::extractTypes(const json::Object& json)
{
	extractEnumerations(json);
	extractStructures(json);
	extractTypeAliases(json);
}

void MetaModel::extractMessages(const json::Object& json)
{
	extractRequests(json);
	extractNotifications(json);
}

void MetaModel::extractRequests(const json::Object& json)
{
	const auto& requests = json.get("requests").array();

	for(const auto& r : requests)
	{
		const auto& obj = r.object();
		const auto& method = obj.get("method").string();

		if(m_requestsByMethod.contains(method))
			throw std::runtime_error("Duplicate request method: " + method);

		m_requestsByMethod[method].extract(obj);
	}
}

void MetaModel::extractNotifications(const json::Object& json)
{
	const auto& notifications = json.get("notifications").array();

	for(const auto& r : notifications)
	{
		const auto& obj = r.object();
		const auto& method = obj.get("method").string();

		if(m_notificationsByMethod.contains(method))
			throw std::runtime_error("Duplicate request method: " + method);

		m_notificationsByMethod[method].extract(obj);
	}
}

void MetaModel::insertType(const std::string& name, Type type, std::size_t index)
{
	if(m_typesByName.contains(name))
		throw std::runtime_error("Duplicate type '" + name + '\"');

	auto it = m_typesByName.insert(std::pair(name, TypeIndex(type, index))).first;
	m_typeNames.push_back(it->first);
}

void MetaModel::extractEnumerations(const json::Object& json)
{
	const auto& enumerations = json.get("enumerations").array();

	m_enumerations.resize(enumerations.size());

	for(std::size_t i = 0; i < enumerations.size(); ++i)
	{
		m_enumerations[i].extract(enumerations[i].object());
		insertType(m_enumerations[i].name, Type::Enumeration, i);
	}
}

void MetaModel::extractStructures(const json::Object& json)
{
	const auto& structures = json.get("structures").array();

	m_structures.resize(structures.size());

	for(std::size_t i = 0; i < structures.size(); ++i)
	{
		m_structures[i].extract(structures[i].object());
		insertType(m_structures[i].name, Type::Structure, i);
	}
}

void MetaModel::addTypeAlias(const json::Object& json, const std::string& key, const std::string& typeBaseName)
{
	if(json.contains(key))
	{
		const auto& typeJson = json.get(key).object();

		if(typeJson.get("kind").string() != "reference")
		{
			auto& alias = m_typeAliases.emplace_back();
			alias.name = typeBaseName + capitalizeString(key);
			alias.type = lspgen::Type::createFromJson(typeJson);
			alias.documentation = extractDocumentation(typeJson);
			insertType(alias.name, Type::TypeAlias, m_typeAliases.size() - 1);
		}
	}
}

void MetaModel::extractTypeAliases(const json::Object& json)
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
		const auto& typeBaseName = obj.get("method").string();

		addTypeAlias(obj, "result", typeBaseName);
		addTypeAlias(obj, "params", typeBaseName);
		addTypeAlias(obj, "partialResult", typeBaseName);
		addTypeAlias(obj, "errorData", typeBaseName);
		addTypeAlias(obj, "registrationOptions", typeBaseName);
	}

	const auto& notifications = json.get("notifications").array();

	for(const auto& n : notifications)
	{
		const auto& obj = n.object();
		const auto& typeBaseName = obj.get("method").string();
		addTypeAlias(obj, "params", typeBaseName);
		addTypeAlias(obj, "registrationOptions", typeBaseName);
	}
}

} // namespace lspgen
