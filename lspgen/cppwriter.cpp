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

void CppWriter::writeBlockStart()
{
	writeLine("{");
	++m_indent;
}

void CppWriter::writeBlockEnd(bool addSemicolon, bool addEmptyLine)
{
	--m_indent;

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
	writeBlockStart();
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

	writeBlockStart();
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

void CppWriter::writeVariable(std::string_view name, std::string_view type, std::string_view initializer, VariableKind kind)
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

	writeLine(";");
}

auto CppWriter::text() const -> std::string_view
{
	return m_text;
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
