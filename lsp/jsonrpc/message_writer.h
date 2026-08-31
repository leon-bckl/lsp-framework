#pragma once

#include <lsp/json/writer.h>
#include "jsonrpc.h"

namespace lsp::jsonrpc{

/*
 * RequestWriter
 */

class RequestWriter{
public:
	void finalize();

	[[nodiscard]] static auto writeRequest(json::ObjectWriter&& writer, const MessageId& id, std::string_view method) -> RequestWriter;
	[[nodiscard]] static auto writeNotification(json::ObjectWriter&& writer, std::string_view method) -> RequestWriter;

	template<typename F, typename T>
	requires std::invocable<F, std::string_view, const T&, json::ObjectWriter&>
	void writeParams(F&& writer, const T& value)
	{
		writer("params", value, m_paramsWriter);
	}

private:
	json::ObjectWriter m_paramsWriter;

	RequestWriter(json::ObjectWriter&& writer, std::string_view method, const MessageId* id);
};

/*
 * ResponseWriter
 */

class ResponseWriter{
public:
	ResponseWriter(ResponseWriter&& other) noexcept;
	~ResponseWriter();

	ResponseWriter& operator=(ResponseWriter&& other) noexcept;

	ResponseWriter(const ResponseWriter&)            = delete;
	ResponseWriter& operator=(const ResponseWriter&) = delete;

	void finalize();

	[[nodiscard]] static auto writeResponse(json::ObjectWriter&& objectWriter, const MessageId& id) -> ResponseWriter;
	[[nodiscard]] static auto writeError(json::ObjectWriter&& objectWriter, const MessageId& id, int code, std::string_view message) -> ResponseWriter;

	template<typename F, typename T>
	requires std::invocable<F, std::string_view, T&&, json::ObjectWriter&>
	void writeData(F&& writer, T&& value)
	{
		m_hasData = true;

		if(m_errorWriter.has_value())
			writer("data", std::forward<T>(value), *m_errorWriter);
		else
			writer("result", std::forward<T>(value), m_resultWriter);
	}

private:
	bool                              m_hasData = false;
	json::ObjectWriter                m_resultWriter;
	std::optional<json::ObjectWriter> m_errorWriter;

	ResponseWriter(json::ObjectWriter&& writer, const MessageId& id);
};

/*
 * BatchWriter
 */

class BatchWriter{
public:
	BatchWriter(json::ArrayWriter&& writer);

	void finalize();
	bool batchIsEmpty() const;

	[[nodiscard]] auto writeRequest(const MessageId& id, std::string_view method) -> RequestWriter;
	[[nodiscard]] auto writeNotification(std::string_view method) -> RequestWriter;
	[[nodiscard]] auto writeResponse(const MessageId& id) -> ResponseWriter;
	[[nodiscard]] auto writeError(const MessageId& id, int code, std::string_view message) -> ResponseWriter;

private:
	bool              m_empty = true;
	json::ArrayWriter m_writer;
};

} // namespace lsp::jsonrpc
