# lsp-framework

This is an implementation of the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) in C++. It can be used to implement both servers and clients that communicate using the LSP.

## Building

There aren't any external dependencies except for `cmake` and a compiler that supports C++20.

The project is built as a static library. LSP type definitions, messages and serialization boilerplate are generated from the official [meta model](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/metaModel/metaModel.json) during the build.

`cmake -S . -B build && cmake --build build --parallel`

## Overview

The goal of this library is to make implementing LSP servers and clients easy and type safe.
All LSP types and messages are proper C++ structs. There's no need to manually read or write JSON which is annyoing and error-prone. The framework handles serialization and deserialization automatically.

All messages can be found in the generated `<lsp/messages.h>` header with requests inside the `lsp::requests` and notifications inside the `lsp::notifications` namespace respectively. All types like the message parameters or results can be found in `<lsp/types.h>`.

## Usage

First you need to establish a connection to the client or server you want to communicate with. The library provides communication via `stdio` only right now. If you need another way of communicating with the other process (e.g. sockets or named pipes) you need to extend `lsp::io::Stream` and implement the `read` and `write` methods.

Create an `lsp::Connection` using a stream and then an `lsp::MessageHandler` with the connection:

```cpp
#include <lsp/messages.h> // Generated message definitions
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

auto connection     = lsp::Connection(lsp::io::standardIO());
auto messageHandler = lsp::MessageHandler(connection);
```

The message handler is the core of the framework. It is used to register callbacks for incoming requests and notifications as well as send outgoing requests and notifications.

Once the callbacks are registered it is necessary to enter the message processing loop which needs to call `lsp::MessageHandler::processIncomingMessages`. This call will block until input becomes available.

```cpp
while(running)
    messageHandler.processIncomingMessages();
```

### Request Callbacks

Request handlers are registered using the `lsp::MessageHandler::add` method. The following example registers a callback for the `initialize` request which is the first request sent to a server by a client in the LSP:

```cpp
messageHandler.add<lsp::requests::Initialize>(
      [](const lsp::MessageId& id, lsp::requests::Initialize::Params&& params)
      {
         return lsp::requests::Initialize::Result{
            .serverInfo = lsp::InitializeResultServerInfo{
                .name    = "Language Server",
                .version = "1.0.0"
            },
            .capabilities = {
                .positionEncoding = lsp::PositionEncodingKind::UTF16
            }
         };
      });
```

The template parameter is the type of the message the callback is for and determines its signature. All request callbacks always have an `lsp::MessageId` as the first parameter. The second parameter are the `MessageType::Params` (only if the message has parameters). They are passed as an rvalue reference so the callback can safely move data out to avoid expensive copies like for example the entire text of a document from the `textDocument/didOpen` notification.

The callback returns the result of the request (`MessageType::Result`) if it has one.

`MessageHandler::add` returns a reference to the handler itself in order to easily chain multiple callback registrations without repeating the handler instance over and over:

```cpp
messageHandler.add<>().add<>().add<>();
```

### Notification Callbacks

Notfication callbacks are registered in exactly the same way as request callbacks. The only difference is that they do not have a message id as the first parameter and never return a result:

```cpp
messageHandler.add<lsp::notifications::Exit>([](){ ... });

```

### Asynchronous Request Callbacks

Some requests might take a longer time to process than others. In order to not stall the handling of other incoming messages, it is possible to do the processing asynchronously.

Asynchronous callbacks work exactly the same as regular request callbacks with the only difference being that they return a `std::future<MessageType::Result>`. Processing happens in a worker thread inside of the message handler. Worker threads are only created if there are asynchronous request handlers. Otherwise the handler will not create any extra threads. 

```cpp
messageHandler.add<lsp::requests::TextDocument_Hover>([](const lsp::MessageId& id, lsp::requests::TextDocument_Hover::Params&& params)
      {
          return std::async(std::launch::deferred, [](lsp::requests::TextDocument_Hover::Params&& params)
          {
              auto result = lsp::TextDocument_HoverResult{};
              // init hover result here
              return result;
          }, std::move(params));
      }
```

### Returning Error Responses

If an error occurs while processing the request and no proper result can be provided an error response should be sent back. In order to do that simply throw an `lsp::RequestError` from inside of the callback (`#include <lsp/error.h>`):

```cpp
throw lsp::RequestError(lsp::ErrorCodes::InvalidParams, "Invalid parameters received");
```

### Sending Requests

Requests are sent using the `lsp::MessageHandler::sendRequest` method. Just like with registering the callbacks, it takes a template parameter for the message type. An `lsp::MessageId` identifying the sent request is returned.

There are two versions of this method:

One returns a `std::future<MessageType::Result>` in addition to the message id. The future will become ready once a response was received. Don't call `std::future::wait` on the same thread that calls `processIncomingMessages` since it would block. If an error response was returned, the future will rethrow it so make sure to handle that case.

```cpp
auto params = lsp::requests::TextDocument_Diagnostic::Params{...}
auto [id, result] = messageHandler.sendRequest<lsp::requests::TextDocument_Diagnostic>(std::move(params));
```

The second version allows specifying callbacks for the success and error cases. The success callback has a `MessageType::Result` parameter and the error callbacks an `lsp::Error` containing the error code and message from the response. Uncaught exceptions inside of the callbacks will abort the connection.


```cpp
auto params = lsp::requests::TextDocument_Diagnostic::Params{...}
auto messageId = messageHandler.sendRequest<lsp::requests::TextDocument_Diagnostic>(
	std::move(params),
	[](lsp::requests::TextDocument_Diagnostic::Result&& result){
		// Called on success with the payload of the response
	},
	[](const lsp::Error& error){
		// Called in the error case
	});
```

### Sending Notifications

Notifications are sent using `lsp::MessageHandler::sendNotification`. They don't have a message id and don't receive a response which means all you need are the parameters if the notification has any:

```cpp
// With params
auto params = lsp::notifications::TextDocument_PublishDiagnostics::Params{...};
messageHandler.sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(std::move(params));

// Without params
messageHandler.sendNotification<lsp::notifications::Exit>();

```

## Starting a Server Process

When implementing an LSP client it usually is responsible for creating the server process. This can be done with the `lsp::Process` class. It has a member `stdIO` which can be used to initialize a connection via the standard input and output of the process.

```cpp
#include <lsp/process.h>
#include <lsp/connection.h>

auto process    = lsp::Process("/usr/bin/clangd", {/*args*/});
auto connection = lsp::Connection(process.stdIO());
```

## License

This project is licensed under the [MIT License](LICENSE).
