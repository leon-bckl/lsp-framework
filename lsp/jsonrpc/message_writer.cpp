#include <utility>
#include "message_writer.h"

namespace lsp::jsonrpc{
namespace{

void writeMessageBase(json::ObjectWriter& writer, const MessageId* id = nullptr, std::string_view method = {})
{
	writer.write("jsonrpc", "2.0");

	if(id)
		writer.write("id", *id);

	if(!method.empty())
		writer.write("method", method);
}

} // namespace

/*
 * RequestWriter
 */

RequestWriter::RequestWriter(json::ObjectWriter&& writer, std::string_view method, const MessageId* id)
	: m_paramsWriter{std::move(writer)}
{
	writeMessageBase(m_paramsWriter, id, method);
}

void RequestWriter::finalize()
{
	m_paramsWriter.finalize();
}

auto RequestWriter::writeRequest(json::ObjectWriter&& writer, const MessageId& id, std::string_view method) -> RequestWriter
{
	return RequestWriter(std::move(writer), method, &id);
}

auto RequestWriter::writeNotification(json::ObjectWriter&& writer, std::string_view method) -> RequestWriter
{
	return RequestWriter(std::move(writer), method, nullptr);
}

/*
 * ResponseWriter
 */

ResponseWriter::ResponseWriter(json::ObjectWriter&& writer, const MessageId& id)
	: m_resultWriter{std::move(writer)}
{
	writeMessageBase(m_resultWriter, &id);
}

ResponseWriter::ResponseWriter(ResponseWriter&& other) noexcept
	: m_hasData{std::exchange(other.m_hasData, true)}
	, m_resultWriter{std::move(other.m_resultWriter)}
	, m_errorWriter{std::move(other.m_errorWriter)}
{
}

ResponseWriter::~ResponseWriter()
{
	finalize();
}

auto ResponseWriter::operator=(ResponseWriter&& other) noexcept -> ResponseWriter&
{
	finalize();

	m_hasData      = std::exchange(other.m_hasData, true);
	m_resultWriter = std::move(other.m_resultWriter);
	m_errorWriter  = std::move(other.m_errorWriter);

	return *this;
}

void ResponseWriter::finalize()
{
	if(!m_hasData && !m_errorWriter.has_value())
		m_resultWriter.write("result", nullptr);

	m_hasData = true;

	if(m_errorWriter.has_value())
		m_errorWriter->finalize();

	m_resultWriter.finalize();
}

auto ResponseWriter::writeResponse(json::ObjectWriter&& objectWriter, const MessageId& id) -> ResponseWriter
{
	return ResponseWriter(std::move(objectWriter), id);
}

auto ResponseWriter::writeError(json::ObjectWriter&& objectWriter, const MessageId& id, int code, std::string_view message) -> ResponseWriter
{
	auto responseWriter = ResponseWriter(std::move(objectWriter), id);
	auto errorWriter    = responseWriter.m_resultWriter.beginObject("error");

	errorWriter.write("code", code);
	errorWriter.write("message", message);
	responseWriter.m_errorWriter = std::move(errorWriter);

	return responseWriter;
}

/*
 * BatchWriter
 */

BatchWriter::BatchWriter(json::ArrayWriter&& writer)
	: m_writer{std::move(writer)}
{
}

void BatchWriter::finalize()
{
	m_writer.finalize();
}

auto BatchWriter::batchIsEmpty() const -> bool
{
	return m_empty;
}

auto BatchWriter::writeRequest( const MessageId& id, std::string_view method) -> RequestWriter
{
	m_empty = false;
	return RequestWriter::writeRequest(m_writer.beginObject(), id, method);
}

auto BatchWriter::writeNotification(std::string_view method) -> RequestWriter
{
	m_empty = false;
	return RequestWriter::writeNotification(m_writer.beginObject(), method);
}

auto BatchWriter::writeResponse(const MessageId& id) -> ResponseWriter
{
	m_empty = false;
	return ResponseWriter::writeResponse(m_writer.beginObject(), id);
}

auto BatchWriter::writeError(const MessageId& id, int code, std::string_view message) -> ResponseWriter
{
	m_empty = false;
	return ResponseWriter::writeError(m_writer.beginObject(), id, code, message);
}

} // namespace lsp::jsonrpc
