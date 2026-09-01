#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lspgen{

class CppWriter{
public:
	CppWriter(int initialIndent = 0);
	CppWriter(std::string& buffer, int initialIndent = 0);

	void reset();
	void reset(std::string& buffer);
	void write(std::string_view text, bool indent = true);
	void writeLine(std::string_view text, bool indent = true);
	void writeDocComment(std::string_view title, std::string_view description);
	void writeEmptyLine();
	void writeNamespaceStart(std::string_view name);
	void writeNamespaceEnd(std::string_view name, bool addEmptyLine = true);
	void writeBlockStart(bool newLine);
	void writeBlockEnd(bool addSemicolon, bool addEmptyLine);
	void writeEnumStart(std::string_view name);
	void writeEnumEnd();
	void writeStructStart(std::string_view name, std::string_view base = {});
	void writeStructEnd();
	void writeClassStart(std::string_view name, std::string_view base = {});
	void writeClassEnd();
	void writeTypedef(std::string_view name, std::string_view type);

	enum VariableKind{
		VarPlain           = 0x0,
		VarStatic          = 0x1,
		VarConstExpr       = 0x2,
		VarConst           = 0x4,
		VarStaticConstExpr = VarStatic | VarConstExpr
	};

	void writeVariable(std::string_view name, std::string_view type, std::string_view initializer = {}, int kind = VarPlain, bool addSemicolon = true);

	struct FuncParam{
		std::string type;
		std::string name;
		std::string defaultValue = {};
	};

	enum FunctionKind{
		FuncPlain    = 0x0,
		FuncInline   = 0x1,
		FuncStatic   = 0x2,
		FuncConst    = 0x4,
		FuncNoexcept = 0x8,
		FuncOverride = 0x10,
	};

	using FuncParamList = std::vector<FuncParam>;

	void writeFuncSig(std::string_view name, std::string_view returnType, const FuncParamList& params, int kind = FuncPlain);
	void indent(){ ++m_indent; }
	void outdent(){ --m_indent; }
	auto text() const -> std::string_view;
	auto currentIndent() const -> int;

	enum TypeKind{
		TypeConst  = 0x1,
		TypePtr    = 0x2,
		TypeRef    = 0x4,
		TypeRvalue = 0x10,
	};

	static auto type(std::string_view baseName, int kind) -> std::string;
	static auto upperCaseIdentifier(std::string_view str) -> std::string;
	static auto lowerCaseIdentifier(std::string_view str) -> std::string;

private:
	const int    m_initialIndent = 0;
	int          m_indent        = m_initialIndent;
	std::string* m_buffer = nullptr;
	std::string  m_internalBuffer; // used when no buffer was provided on construction

	void writeIndent();
};

} // namespace lspgen
