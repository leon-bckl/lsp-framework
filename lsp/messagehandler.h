#pragma once

#include <mutex>
#include <future>
#include <vector>
#include <utility>
#include <concepts>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <lsp/jsonrpc/jsonrpc.h>
#include "messagebase.h"

namespace lsp{
namespace types{
class ErrorCodes;
}

template<typename T>
concept HasParams = requires{
	typename T::Params;
};

template<typename T>
concept HasResult = requires{
	typename T::Result;
};

class Connection;

class MessageHandler{
private:
	class ResponseResultBase;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;

public:
	MessageHandler(Connection& connection) : m_connection{connection}{}

	void processIncomingMessages();

	/*
	 * Callback registration
	 */

	template<typename MessageType>
	requires HasParams<MessageType> && (!HasResult<MessageType>)
	MessageHandler& handler(const std::function<void(const typename MessageType::Params&)>& handlerFunc);

	template<typename MessageType>
	requires HasParams<MessageType> && HasResult<MessageType>
	MessageHandler& handler(const std::function<typename MessageType::Result(const typename MessageType::Params&)>& handlerFunc);

	template<typename MessageType>
	requires HasResult<MessageType> && (!HasResult<MessageType>)
	MessageHandler& handler(const std::function<typename MessageType::Result()>& handlerFunc);

	template<typename MessageType>
	requires (!HasParams<MessageType>) && (!HasResult<MessageType>)
	MessageHandler& handler(const std::function<void()>& handlerFunc);

	/*
	 * sendRequest
	 */

	template<typename MessageType>
	requires HasParams<MessageType> && HasResult<MessageType>
	[[nodiscard]] std::future<typename MessageType::Result> sendRequest(const std::string& id, const typename MessageType::Params& params);

	template<typename MessageType>
	requires HasResult<MessageType> && (!HasParams<MessageType>)
	[[nodiscard]] std::future<typename MessageType::Result> sendRequest(const std::string& id);

	/*
	 * sendNotification
	 */

	template<typename MessageType>
	requires (!HasParams<MessageType>) && (!HasResult<MessageType>)
	void sendNotification(){
		sendNotification(MessageType::method);
	}

	template<typename MessageType>
	requires HasParams<MessageType> && (!HasResult<MessageType>)
	void sendNotification(const typename MessageType::Params& params){
		sendNotification(MessageType::method, tojson(params));
	}

	bool handlesRequest(messages::Method method);

private:
	using HandlerWrapper = std::function<jsonrpc::ResponsePtr(const jsonrpc::MessageId&, const json::Any&)>;
	Connection&                                               m_connection;
	std::vector<HandlerWrapper>                               m_requestHandlers;
	std::mutex                                                m_requestMutex;
	std::unordered_map<jsonrpc::MessageId, ResponseResultPtr> m_pendingRequests;

	jsonrpc::ResponsePtr processRequest(const jsonrpc::Request& request);
	void processResponse(const jsonrpc::Response& response);
	jsonrpc::ResponsePtr processMessage(const jsonrpc::Message& message);
	jsonrpc::MessageBatch processBatch(const jsonrpc::MessageBatch& batch);
	void addHandler(messages::Method method, HandlerWrapper&& handlerFunc);
	void sendRequest(messages::Method method, const jsonrpc::MessageId& id, ResponseResultPtr result, const std::optional<json::Any>& params = std::nullopt);
	void sendNotification(messages::Method method, const std::optional<json::Any>& params = std::nullopt);
	void sendErrorMessage(types::ErrorCodes code, const std::string& message);

	template<typename T>
	jsonrpc::ResponsePtr createResponse(const jsonrpc::MessageId& id, const T& result){
		return jsonrpc::createResponse(id, toJson(result));
	}

	/*
	 * Response result wrapper
	 */

	class ResponseResultBase{
	public:
		virtual ~ResponseResultBase() = default;
		virtual void setValueFromJson(const json::Any& json) = 0;
		virtual void setException(std::exception_ptr e) = 0;
	};

	template<typename T>
	class ResponseResult : public ResponseResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(const json::Any& json) override{
			T value;
			fromJson(json, value);
			m_promise.set_value(std::move(value));
		}

		void setException(std::exception_ptr e) override{
			m_promise.set_exception(e);
		}

	private:
		std::promise<T> m_promise;
	};
};

template<typename MessageType>
requires HasParams<MessageType> && (!HasResult<MessageType>)
MessageHandler& MessageHandler::handler(const std::function<void(const typename MessageType::Params&)>& handlerFunc){
	addHandler(MessageType::MessageMethod, [handlerFunc](const jsonrpc::MessageId&, const json::Any& json){
		typename MessageType::Params params;
		fromJson(json, params);
		handlerFunc(params);
		return nullptr;
	});

	return *this;
}

template<typename MessageType>
requires HasParams<MessageType> && HasResult<MessageType>
MessageHandler& MessageHandler::handler(const std::function<typename MessageType::Result(const typename MessageType::Params&)>& handlerFunc){
	addHandler(MessageType::MessageMethod, [this, handlerFunc](const jsonrpc::MessageId& id, const json::Any& json){
		typename MessageType::Params params;
		fromJson(json, params);
		return createResponse(id, handlerFunc(params));
	});

	return *this;
}

template<typename MessageType>
requires HasResult<MessageType> && (!HasResult<MessageType>)
MessageHandler& MessageHandler::handler(const std::function<typename MessageType::Result()>& handlerFunc){
	addHandler(MessageType::MessageMethod, [this, handlerFunc](const jsonrpc::MessageId& id, const json::Any&){
		auto result = handlerFunc();
		return createResponse(id, handlerFunc());
	});

	return *this;
}

template<typename MessageType>
requires (!HasParams<MessageType>) && (!HasResult<MessageType>)
MessageHandler& MessageHandler::handler(const std::function<void()>& handlerFunc){
	addHandler(MessageType::MessageMethod, [handlerFunc](const jsonrpc::MessageId&, const json::Any&){
		handlerFunc();
		return nullptr;
	});

	return *this;
}

template<typename MessageType>
requires HasParams<MessageType> && HasResult<MessageType>
std::future<typename MessageType::Result> MessageHandler::sendRequest(const std::string& id, const typename MessageType::Params& params){
	auto result = std::make_unique<ResponseResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::MessageMethod, id, std::move(result), toJson(params));
	return future;
}

template<typename MessageType>
requires HasResult<MessageType> && (!HasParams<MessageType>)
std::future<typename MessageType::Result> MessageHandler::sendRequest(const std::string& id){
	auto result = std::make_unique<ResponseResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::MessageMethod, id, std::move(result));
	return future;
}

} // namespace lsp
