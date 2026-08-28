#include <algorithm>
#include "cppwriter.h"
#include "util.h"

namespace lspgen{

CppWriter::CppWriter(int initialIndent)
	: m_indent{initialIndent}
{
}

void CppWriter::reset()
{
	m_text.clear();
}

void CppWriter::write(std::string_view text, bool indent)
{
	if(indent)
		writeIndent();

	m_text += text;
}

void CppWriter::writeLine(std::string_view text, bool indent)
{
	write(text, indent);
	m_text += '\n';
}

void CppWriter::writeDocComment(std::string_view title, std::string_view description)
{
	if(title.empty() && description.empty())
		return;

	writeLine("/*");

	if(!title.empty())
		writeLine(" * " + std::string(title));

	const auto descLines = splitStringView(description, "\n");

	if(!title.empty() && !descLines.empty())
		writeLine(" *");

	for(const auto& line : descLines)
		writeLine(" * " + replaceString(replaceString(line, "/*", "/_*"), "*/", "*_/"));

	writeLine(" */");
}

void CppWriter::writeEmptyLine()
{
	if(m_text.empty() || m_text.back() != '\n')
		m_text += '\n';

	m_text += '\n';
}

void CppWriter::writeNamespaceStart(std::string_view name)
{
	m_text += "namespace";

	if(!name.empty())
	{
		m_text += ' ';
		m_text += name;
	}

	m_text += "{\n\n";
}

void CppWriter::writeNamespaceEnd(std::string_view name)
{
	m_text += "} // namespace ";
	m_text += name;
	m_text += "\n\n";
}

void CppWriter::writeBlockStart(bool newLine)
{
	if(newLine)
		writeLine("");

	writeLine("{");
	indent();
}

void CppWriter::writeBlockEnd(bool addSemicolon, bool addEmptyLine)
{
	outdent();

	if(addSemicolon)
		writeLine("};");
	else
		writeLine("}");

	if(addEmptyLine)
		writeEmptyLine();
}

void CppWriter::writeEnumStart(std::string_view name)
{
	write("enum class ");
	write(name);
	writeBlockStart(false);
}

void CppWriter::writeEnumEnd()
{
	writeBlockEnd(true, true);
}

void CppWriter::writeStructStart(std::string_view name, std::string_view base)
{
	write("struct ");
	write(name);

	if(!base.empty())
	{
		write(" : ");
		write(base );
	}

	writeBlockStart(false);
}

void CppWriter::writeStructEnd()
{
	writeBlockEnd(true, true);
}

void CppWriter::writeTypedef(std::string_view name, std::string_view type)
{
	write("using ");
	write(name);
	write(" = ");
	write(type);
	writeLine(";");
}

void CppWriter::writeIndent()
{
	if(m_text.empty() || m_text.back() == '\n')
	{
		for(int i = 0; i < m_indent; ++i)
			m_text += '\t';
	}
}

void CppWriter::writeVariable(std::string_view name, std::string_view type, std::string_view initializer, int kind, bool addSemicolon)
{
	if(kind & VarStatic)
		write("static ");

	if(kind & VarConstExpr)
		write("constexpr ");

	if(kind & VarConst)
		write("const ");

	write(type);
	write(" ");
	write(name);

	if(!initializer.empty())
	{
		write(" = ");
		write(initializer);
	}

	if(addSemicolon)
		writeLine(";");
}

void CppWriter::writeFuncSig(std::string_view name, std::string_view returnType, const FuncParamList& params, int kind)
{
	if(kind & FuncInline)
		write("inline ");

	if(kind & FuncStatic)
		write("static ");

	if(!returnType.empty())
	{
		write(returnType);
		write(" ");
	}

	write(name);

	write("(");

	bool first = true;

	for(const auto& param : params)
	{
		if(!first)
			write(", ");

		first = false;
		write(param.type);
		write(" ");
		write(param.name);
	}

	write(")");

	if(kind & FuncConst)
		write(" const");

	if(kind & FuncNoexcept)
		write(" noexcept");

	if(kind & FuncOverride)
		write(" override");
}

auto CppWriter::text() const -> std::string_view
{
	return m_text;
}

auto CppWriter::type(std::string_view baseName, int kind) -> std::string
{
	auto type = std::string();

	if(kind & TypeConst)
		type += "const ";

	type += baseName;

	if(kind & TypePtr)
		type += "*";

	if(kind & TypeRef)
		type += "&";

	if(kind & TypeRvalue)
		type += "&&";

	return type;
}

auto CppWriter::upperCaseIdentifier(std::string_view str) -> std::string
{
	if(str.starts_with('$'))
		str.remove_prefix(1);

	const auto parts = splitStringView(str, "/", true);
	auto       id    = joinStrings(parts, {}, capitalizeString);

	std::transform(id.cbegin(), id.cend(), id.begin(), [](char c)
	{
		if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9'))
			return '_';

		return c;
	});

	return id;
}

auto CppWriter::lowerCaseIdentifier(std::string_view str) -> std::string
{
	return uncapitalizeString(upperCaseIdentifier(str));
}

} // namespace lspgen
