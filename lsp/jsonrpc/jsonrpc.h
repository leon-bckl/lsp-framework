#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <lsp/json/json.h>

namespace lsp::jsonrpc{
	struct Message{
		std::string jsonrpc = "2.0";

		virtual ~Message() = default;
		virtual json::Any toJson() const = 0;
		virtual bool isRequest() const{ return false; }
		virtual bool isResponse() const{ return false; }
	};

	using MessagePtr = std::unique_ptr<Message>;
	using MessageBatch = std::vector<MessagePtr>;
	using MessageId = std::variant<json::String, json::Integer, json::Null>;

	/*
	 * Request
	 */

	struct Request final : Message{
		std::optional<MessageId> id;
		std::string              method;
		std::optional<json::Any> params;

		json::Any toJson() const override;
		bool isRequest() const override{ return true; }
		bool isNotification() const{ return !id.has_value(); }
	};

	using RequestPtr = std::unique_ptr<Request>;

	/*
	 * Response
	 */

	struct ResponseError{
		json::Integer            code;
		json::String             message;
		std::optional<json::Any> data;
	};

	struct Response final : Message{
		MessageId                    id = json::Null{};
		std::optional<json::Any>     result;
		std::optional<ResponseError> error;

		json::Any toJson() const override;
		bool isResponse() const override{ return true; }
	};

	using ResponsePtr = std::unique_ptr<Response>;

	/*
	 * Error thrown when a message has an invalid structure
	 */
	class ProtocolError : public std::runtime_error{
	public:
		using std::runtime_error::runtime_error;
	};

	std::variant<MessagePtr, MessageBatch> messageFromJson(const json::Any& json);
	RequestPtr createRequest(const MessageId& id, const std::string& method, const std::optional<json::Any>& params = std::nullopt);
	RequestPtr createNotification(const std::string& method, const std::optional<json::Any>& params = std::nullopt);
	ResponsePtr createResponse(const MessageId& id, const json::Any& result);
	ResponsePtr createErrorResponse(const MessageId& id, json::Integer errorCode, const json::String& message, const std::optional<json::Any>& data);

}
