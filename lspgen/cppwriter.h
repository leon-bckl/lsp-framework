#pragma once

#include <string>
#include <string_view>

namespace lspgen{

class CppWriter{
public:
	CppWriter(int initialIndent = 0);

	void reset();
	void write(std::string_view text, bool indent = true);
	void writeLine(std::string_view text, bool indent = true);
	void writeDocComment(std::string_view title, std::string_view description);
	void writeEmptyLine();
	void writeNamespaceStart(std::string_view name);
	void writeNamespaceEnd(std::string_view name);
	void writeBlockStart();
	void writeBlockEnd(bool addSemicolon, bool addEmptyLine);
	void writeEnumStart(std::string_view name);
	void writeEnumEnd();
	void writeStructStart(std::string_view name, std::string_view base = {});
	void writeStructEnd();
	void writeTypedef(std::string_view name, std::string_view type);

	enum VariableKind{
		VarPlain           = 0x0,
		VarStatic          = 0x1,
		VarConstExpr       = 0x2,
		VarConst           = 0x4,
		VarStaticConstExpr = VarStatic | VarConstExpr
	};

	void writeVariable(std::string_view name, std::string_view type, std::string_view initializer = {}, VariableKind kind = VarPlain);
	auto text() const -> std::string_view;

	static auto upperCaseIdentifier(std::string_view str) -> std::string;
	static auto lowerCaseIdentifier(std::string_view str) -> std::string;

private:
	std::string m_text;
	int         m_indent = 0;

	void writeIndent();
};

} // namespace lspgen
