#include <algorithm>
#include "cpp_writer.h"
#include "util.h"

namespace lspgen{

CppWriter::CppWriter(int initialIndent)
	: m_initialIndent{initialIndent}
	, m_indent{initialIndent}
	, m_buffer{&m_internalBuffer}
{
}

CppWriter::CppWriter(std::string& buffer, int initialIndent)
	: m_initialIndent{initialIndent}
	, m_indent{initialIndent}
	, m_buffer{&buffer}
{
}

void CppWriter::reset()
{
	reset(m_internalBuffer);
}

void CppWriter::reset(std::string& buffer)
{
	m_internalBuffer.clear();
	m_indent = m_initialIndent;
	m_buffer = &buffer;
}

void CppWriter::write(std::string_view text, bool indent)
{
	if(indent)
		writeIndent();

	(*m_buffer) += text;
}

void CppWriter::writeLine(std::string_view text, bool indent)
{
	write(text, indent);
	(*m_buffer) += '\n';
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
	if(m_buffer->empty() || m_buffer->back() != '\n')
		(*m_buffer) += '\n';

	(*m_buffer) += '\n';
}

void CppWriter::writeNamespaceStart(std::string_view name)
{
	(*m_buffer) += "namespace";

	if(!name.empty())
	{
		(*m_buffer) += ' ';
		(*m_buffer) += name;
	}

	(*m_buffer) += "{\n\n";
}

void CppWriter::writeNamespaceEnd(std::string_view name, bool addEmptyLine)
{
	(*m_buffer) += "} // namespace ";
	(*m_buffer) += name;

	if(addEmptyLine)
		writeEmptyLine();
	else
		(*m_buffer) += '\n';
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
	// Strip newlines before block end
	auto size = m_buffer->size();

	while(size > 2)
	{
		if((*m_buffer)[size - 1] != '\n' || (*m_buffer)[size - 2] != '\n')
			break;

		--size;
	}

	m_buffer->resize(size);

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

void CppWriter::writeClassStart(std::string_view name, std::string_view base)
{
	write("class ");
	write(name);

	if(!base.empty())
	{
		write(" : public ");
		write(base );
	}

	writeBlockStart(false);
}

void CppWriter::writeClassEnd()
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
	if(m_buffer->empty() || m_buffer->back() == '\n')
	{
		for(int i = 0; i < m_indent; ++i)
			(*m_buffer) += '\t';
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

	const auto returnsVoid = returnType == "void";

	if(!returnType.empty())
	{
		if(returnsVoid)
			write(returnType);
		else
			write("auto");

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
		write(param.type + ' ' + param.name);

		if(!param.defaultValue.empty())
			write(" = " + param.defaultValue);
	}

	write(")");

	if(kind & FuncConst)
		write(" const");

	if(kind & FuncNoexcept)
		write(" noexcept");

	if(kind & FuncOverride)
		write(" override");

	if(!returnsVoid && !returnType.empty())
	{
		write(" -> ");
		write(returnType);
	}
}

auto CppWriter::text() const -> std::string_view
{
	return *m_buffer;
}

auto CppWriter::currentIndent() const -> int
{
	return m_indent;
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
