#pragma once

#include <lsp/json/writer.h>
#include "jsonrpc.h"

namespace lsp::jsonrpc{

/*
 * RequestWriter
 */

class RequestWriter{
public:
	[[nodiscard]] static RequestWriter writeRequest(json::ObjectWriter&& writer, const MessageId& id, std::string_view method);
	[[nodiscard]] static RequestWriter writeNotification(json::ObjectWriter&& writer, std::string_view method);

	[[nodiscard]] json::ObjectWriter writeParamsObject();
	[[nodiscard]] json::ArrayWriter  writeParamsArray();

private:
	json::ObjectWriter m_writer;

	RequestWriter(json::ObjectWriter&& writer, std::string_view method, const MessageId* id);
};

/*
 * ResponseWriter
 */

class ResponseWriter{
public:
	[[nodiscard]] static ResponseWriter writeResponse(json::ObjectWriter&& objectWriter, const MessageId& id);
	[[nodiscard]] static ResponseWriter writeError(json::ObjectWriter&& objectWriter, const MessageId& id, int code, std::string_view message);

	[[nodiscard]] json::ObjectWriter writeDataObject();
	[[nodiscard]] json::ArrayWriter  writeDataArray();

	template<json::JsonPrimitive T>
	void writeData(const T& value)
	{
		if(m_errorWriter.has_value())
			m_errorWriter->write("data", value);
		else
			m_writer.write("result", value);
	}

private:
	json::ObjectWriter                m_writer;
	std::optional<json::ObjectWriter> m_errorWriter;

	ResponseWriter(json::ObjectWriter&& writer, const MessageId& id);
};

/*
 * BatchWriter
 */

class BatchWriter{
public:
	BatchWriter(json::ArrayWriter&& writer);

	[[nodiscard]] RequestWriter writeRequest(const MessageId& id, std::string_view method);
	[[nodiscard]] RequestWriter writeNotification(std::string_view method);
	[[nodiscard]] ResponseWriter writeResponse(const MessageId& id);
	[[nodiscard]] ResponseWriter writeError(const MessageId& id, int code, std::string_view message);

private:
	json::ArrayWriter m_writer;
};

} // namespace lsp::jsonrpc
