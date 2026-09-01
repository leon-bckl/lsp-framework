#pragma once

#include <string>
#include <unordered_map>

namespace lspgen{

class Generator{
public:
	void writeFiles() const;

protected:
	[[nodiscard]] auto createFile(std::string_view fileName) -> std::string*;

private:
	std::unordered_map<std::string, std::string> m_files;
};

} // namespace lspgen
