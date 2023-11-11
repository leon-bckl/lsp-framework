#pragma once

#include <memory>
#include <string>
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
	using MessageId = std::variant<json::String, json::Integer, json::Null>;

	/*
	 * Request
	 */

	using RequestParameters = std::variant<json::Object, json::Array>;

	struct Request final : Message{
		std::optional<MessageId>         id;
		std::string                      method;
		std::optional<RequestParameters> params;

		json::Any toJson() const override;
		bool isRequest() const override{ return true; }
		bool isNotification(){ return !id.has_value(); }
	};

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

	MessagePtr messageFromJson(const json::Any& json);

	/*
	 * Error thrown when a message has an invalid structure
	 */
	class ProtocolError : public std::runtime_error{
	public:
		using std::runtime_error::runtime_error;
	};

}
