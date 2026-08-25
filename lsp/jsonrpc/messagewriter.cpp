#include "messagewriter.h"

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
	: m_writer{std::move(writer)}
{
	writeMessageBase(m_writer, id, method);
}

RequestWriter RequestWriter::writeRequest(json::ObjectWriter&& writer, const MessageId& id, std::string_view method)
{
	return RequestWriter(std::move(writer), method, &id);
}

RequestWriter RequestWriter::writeNotification(json::ObjectWriter&& writer, std::string_view method)
{
	return RequestWriter(std::move(writer), method, nullptr);
}

json::ObjectWriter RequestWriter::writeParamsObject()
{
	return m_writer.beginObject("params");
}

json::ArrayWriter RequestWriter::writeParamsArray()
{
	return m_writer.beginArray("params");
}

/*
 * ResponseWriter
 */

ResponseWriter::ResponseWriter(json::ObjectWriter&& writer, const MessageId& id)
	: m_writer{std::move(writer)}
{
	writeMessageBase(m_writer, &id);
}

ResponseWriter ResponseWriter::writeResponse(json::ObjectWriter&& objectWriter, const MessageId& id)
{
	return ResponseWriter(std::move(objectWriter), id);
}

ResponseWriter ResponseWriter::writeError(json::ObjectWriter&& objectWriter, const MessageId& id, int code, std::string_view message)
{
	auto responseWriter = ResponseWriter(std::move(objectWriter), id);
	auto errorWriter    = responseWriter.m_writer.beginObject("error");

	errorWriter.write("code", code);
	errorWriter.write("message", message);
	responseWriter.m_errorWriter = std::move(errorWriter);

	return responseWriter;
}

json::ObjectWriter ResponseWriter::writeDataObject()
{
	if(m_errorWriter.has_value())
		return m_errorWriter->beginObject("data");

	return m_writer.beginObject("result");
}

json::ArrayWriter ResponseWriter::writeDataArray()
{
	if(m_errorWriter.has_value())
		return m_errorWriter->beginArray("data");

	return m_writer.beginArray("result");
}

/*
 * BatchWriter
 */

BatchWriter::BatchWriter(json::ArrayWriter&& writer)
	: m_writer{std::move(writer)}
{
}

RequestWriter BatchWriter::writeRequest( const MessageId& id, std::string_view method)
{
	return RequestWriter::writeRequest(m_writer.beginObject(), id, method);
}

RequestWriter BatchWriter::writeNotification(std::string_view method)
{
	return RequestWriter::writeNotification(m_writer.beginObject(), method);
}

ResponseWriter BatchWriter::writeResponse(const MessageId& id)
{
	return ResponseWriter::writeResponse(m_writer.beginObject(), id);
}

ResponseWriter BatchWriter::writeError(const MessageId& id, int code, std::string_view message)
{
	return ResponseWriter::writeError(m_writer.beginObject(), id, code, message);
}

} // namespace lsp::jsonrpc
