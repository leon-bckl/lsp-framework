#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <unordered_map>
#include <lsp/connection.h>
#include <lsp/error.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/message_base.h>
#include <lsp/request_result.h>
#include <lsp/serialization.h>
#include <lsp/thread_pool.h>

namespace lsp{

using RequestTimestamp = std::chrono::steady_clock::time_point;
using RequestDuration  = std::chrono::steady_clock::duration;

/*
 * MessageHandler
 */

class MessageHandler{
public:
	explicit MessageHandler(Connection connection, unsigned int maxResponseThreads = std::thread::hardware_concurrency() / 2);
	~MessageHandler() = default;

	void processNextMessage();
	void setConnection(Connection connection);

	/*
	 * Callback registration
	 */

	template<typename M, typename F>
	auto on(F&& callback) -> MessageHandler&;

	template<typename M, typename F>
	requires (M::Kind == MessageKind::Request)
	auto onCustom(std::string_view method, F&& callback) -> MessageHandler&;

	template<typename M, typename F>
	requires (M::Kind == MessageKind::Notification)
	auto onCustom(std::string_view method, F&& callback) -> MessageHandler&;

	void remove(const std::string& method);

	/*
	 * sendRequest
	 */

	using ResponseErrorCallback = void(*)(const ResponseError&);
	static void nullErrorCallback(const ResponseError&){}

	template<typename M, typename F, typename E = ResponseErrorCallback>
	requires MessageHasParams<M>
	auto sendRequest(const typename M::Params& params, F&& then, E&& error = nullErrorCallback) -> RequestId;

	template<typename M, typename F, typename E = ResponseErrorCallback>
	requires MessageHasParams<M>
	auto sendCustomRequest(std::string_view method, const typename M::Params& params, F&& then, E&& error = nullErrorCallback) -> RequestId;

	template<typename M, typename F, typename E = ResponseErrorCallback>
	requires (!MessageHasParams<M>)
	auto sendRequest(F&& then, E&& error = nullErrorCallback) -> RequestId;

	template<typename M, typename F, typename E = ResponseErrorCallback>
	requires (!MessageHasParams<M>)
	auto sendCustomRequest(std::string_view method, F&& then, E&& error = nullErrorCallback) -> RequestId;

	template<typename M>
	requires MessageHasParams<M> && MessageHasResult<M>
	[[nodiscard]] auto sendRequest(const typename M::Params& params) -> RequestResult<typename M::Result>;

	template<typename M>
	requires MessageHasParams<M> && MessageHasResult<M>
	[[nodiscard]] auto sendCustomRequest(std::string_view method, const typename M::Params& params) -> RequestResult<typename M::Result>;

	template<typename M>
	requires (!MessageHasParams<M>) && MessageHasResult<M>
	[[nodiscard]] auto sendRequest() -> RequestResult<typename M::Result>;

	template<typename M>
	requires (!MessageHasParams<M>) && MessageHasResult<M>
	[[nodiscard]] auto sendCustomRequest(std::string_view method) -> RequestResult<typename M::Result>;

	/*
	 * sendNotification
	 */

	template<typename M>
	requires MessageHasParams<M> && (!MessageHasResult<M>)
	void sendNotification(const typename M::Params& params);

	template<typename M>
	requires MessageHasParams<M> && (!MessageHasResult<M>)
	void sendCustomNotification(std::string_view method, const typename M::Params& params);

	template<typename M>
	requires (!MessageHasParams<M>) && (!MessageHasResult<M>)
	void sendNotification();

	template<typename M>
	requires (!MessageHasParams<M>) && (!MessageHasResult<M>)
	void sendCustomNotification(std::string_view method);

	/*
	 * Cancelation
	 */

	void cancel(const RequestId& id);
	[[nodiscard]] auto isCanceled(const RequestId& id) -> bool;

	/*
	 * RequestContext
	 */

	class RequestContext{
		friend class MessageHandler;
	public:
		RequestContext(const RequestContext&)     = default;
		RequestContext(RequestContext&&) noexcept = default;
		~RequestContext();

		[[nodiscard]] static auto get() -> const RequestContext&;
		[[nodiscard]] static auto tryGet() -> const RequestContext*;

		[[nodiscard]] auto method() const -> std::string_view{ return m_method; }
		[[nodiscard]] auto id() const -> const RequestId&{ return m_requestId; }
		[[nodiscard]] auto timestamp() const -> RequestTimestamp{ return m_requestTimestamp; }
		[[nodiscard]] auto isCanceled() const -> bool{ return m_messageHandler->isCanceled(id()); }

		void throwIfCanceled() const;

	private:
		MessageHandler*  m_messageHandler = nullptr;
		std::string_view m_method;
		const RequestId& m_requestId;
		RequestTimestamp m_requestTimestamp;

		RequestContext(
			MessageHandler& messageHandler,
			std::string_view method,
			const RequestId& requestId,
			RequestTimestamp requestTimestamp);
	};

	/*
	 * Message log
	 */

	enum class MessageLogLevel{
		Off,
		Info,
		InfoAndPayload
	};

	struct MessageLog{
		struct ErrorData{
			int              code;
			std::string_view message;
		};

		bool                           incoming;
		std::string_view               method;
		std::optional<RequestDuration> requestDuration = {}; // Only set for responses
		std::optional<ErrorData>       error           = {}; // Only set for error responses
		std::optional<RequestId>       id              = {}; // Not set for notifications
		std::optional<std::string>     payload         = {}; // Only set if verbose

		auto isNotification() const{ return !id.has_value(); }
		auto isRequest() const{ return id.has_value() && !requestDuration.has_value(); }
		auto isResponse() const{ return requestDuration.has_value(); }
	};

	using MessageLogCallback = std::function<void(const MessageLog&)>;

	void setMessageLogLevel(MessageLogLevel msgLogLevel);
	void addMessageLogCallback(MessageLogCallback callback);

private:
	class PendingRequestBase;
	using PendingRequestPtr = std::unique_ptr<PendingRequestBase>;
	using HandlerWrapper    = std::function<void(json::Value&&, Connection::BatchSender*)>;

	struct ActiveRequest{
		RequestId id;
		bool      canceled = false;
	};

	// General
	Connection                                      m_connection;
	ThreadPool                                      m_threadPool;
	std::atomic<MessageLogLevel>                    m_msgLogLevel = MessageHandler::MessageLogLevel::Off;
	std::vector<MessageLogCallback>                 m_msgLogCallbacks;
	// Incoming requests
	std::unordered_map<std::string, HandlerWrapper> m_requestHandlersByMethod;
	std::mutex                                      m_activeRequestMutex;
	std::vector<ActiveRequest>                      m_activeRequests;
	// Outgoing requests
	std::mutex                                      m_pendingRequestsMutex;
	std::vector<PendingRequestPtr>                  m_pendingRequests;

	void addActive(const RequestId& id);
	void removeActive(const RequestId& id);
	auto shouldLog() const -> bool;

	template<typename T>
	void dispatchMessageLog(MessageLog& msgLog, const T& payload);

	void dispatchMessageLog(const MessageLog& msgLog);

	template<typename M>
	void sendResponse(RequestResult<typename M::Result>& result, Connection::BatchSender* batchSender);

	void processRequest(jsonrpc::Request&& request, Connection::BatchSender* batchSender);
	void processResponse(jsonrpc::Response&& response);
	void addHandler(std::string_view method, HandlerWrapper&& handlerFunc);
	void addPendingRequest(PendingRequestPtr pendingRequest);
	void sendErrorResponse(
		std::string_view method,
		RequestTimestamp requestTimestamp,
		const RequestId& requestId,
		int errorCode,
		std::string_view errorMessage,
		const std::optional<json::Value>& errorData,
		Connection::BatchSender* batchSender);

	static auto nextUniqueRequestId() -> json::Integer;

	template<typename T>
	struct IsFuture : std::false_type{};

	template<typename... Args>
	struct IsFuture<std::future<Args...>> : std::true_type{};

	/*
	 * Request result wrapper
	 */

	class PendingRequestBase{
	public:
		PendingRequestBase(std::string method, RequestTimestamp timestamp, RequestId id);
		virtual ~PendingRequestBase() = default;
		virtual void setValue(json::Value&& json) = 0;
		virtual void setError(ResponseError&& error) = 0;

		auto method() const -> std::string_view{ return m_method; }
		auto requestTimestamp() const -> RequestTimestamp{ return m_requestTimestamp; }
		auto requestId() const -> const RequestId&{ return m_requestId; }

	protected:
		template<typename T>
		auto setValueFromJson(T& value, json::Value&& json) -> bool;

	private:
		std::string      m_method;
		RequestTimestamp m_requestTimestamp;
		RequestId        m_requestId;
	};

	template<typename T, typename F, typename E>
	class PendingRequestCallback final : public PendingRequestBase{
	public:
		PendingRequestCallback(std::string method, RequestTimestamp timestamp, RequestId id, F&& then, E&& error);

		void setValue(json::Value&& json) override;
		void setError(ResponseError&& error) override;

	private:
		F m_then;
		E m_error;
	};

	template<typename T>
	class PendingRequestFuture final : public PendingRequestBase{
	public:
		PendingRequestFuture(std::string method, RequestTimestamp timestamp, RequestId id);

		auto future() -> std::future<T>{ return m_promise.get_future(); }

		void setValue(json::Value&& json) override;
		void setError(ResponseError&& error) override;

	private:
		std::promise<T> m_promise;
	};
};

} // namespace lsp

#include "message_handler.inl"
