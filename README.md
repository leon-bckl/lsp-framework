# lsp-framework

This is an implementation of the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/overviews/lsp/overview/) in C++. It can be used to implement both servers and clients that communicate using the LSP.

## Overview

The goal of this library is to make implementing LSP servers and clients easy and type safe.
All LSP types and messages are proper C++ structs. There's no need to manually read or write JSON, which is inconvenient and error-prone. The framework handles serialization and deserialization automatically.

The generated `<lsp/messages.h>` header has one struct per message, with requests inside the `lsp::requests` and notifications inside the `lsp::notifications` namespaces.
Each message struct has a `Method` constant and typedefs for its `Params` and `Result` types. All parameter, result and other LSP types are in `<lsp/types.h>`.

These two headers also serve as a protocol reference and have documentation comments for all types and properties.

## Building

There aren't any external dependencies except for `cmake` and a compiler that supports C++20.

The project is built as a static library. LSP type definitions, messages and serialization boilerplate are generated from the official [meta model](https://github.com/microsoft/language-server-protocol/tree/gh-pages/_specifications/lsp) during the build and are written to `<build_dir>/generated/lsp/`.

`cmake -S . -B build && cmake --build build --parallel`

## Usage

The two entry points are the generated `lsp::ServerEndpoint` and `lsp::ClientEndpoint` classes (`<lsp/server_endpoint.h>` and `<lsp/client_endpoint.h>`).
Each is constructed from an `lsp::io::Stream` and exposes a type-safe method for every message it can send, along with an `on...` method to register a handler for every message it can receive.
The framework provides streams for standard I/O (`lsp::io::standardIO()`) and TCP sockets (`lsp::io::Socket`).
To communicate over anything else, subclass `lsp::io::Stream` and implement `read` and `write`. An endpoint does not take ownership of its stream, so the stream must outlive it.

### Writing a Server

Construct an `lsp::ServerEndpoint` from a stream, register handlers, then run the message loop:

```cpp
#include <lsp/io/standard_io.h>
#include <lsp/server_endpoint.h>

int main()
{
  auto endpoint = lsp::ServerEndpoint(lsp::io::standardIO());

  endpoint.onInitialize(
    [](lsp::InitializeParams&& params) -> lsp::InitializeResult
    {
      return {
        .capabilities = {
          .hoverProvider = true,
        },
        .serverInfo = lsp::ServerInfo{.name = "Example Server", .version = "1.0.0"},
      };
    })
  .onTextDocumentHover(
    [](lsp::HoverParams&& params) -> lsp::TextDocumentHoverResult
    {
      return lsp::Hover{
        .contents = lsp::MarkupContent{
          .kind  = lsp::MarkupKind::PlainText,
          .value = "Hover at line " + std::to_string(params.position.line),
        },
      };
    });

  endpoint.runMessageLoop();
}
```

Every message the server can receive has a matching `on<MessageName>` method, named after the LSP method (e.g., `textDocument/hover` becomes `onTextDocumentHover`).
Each `on...` method returns the endpoint, so registrations can be chained.

`runMessageLoop()` processes messages until the client sends `exit` or the connection is closed. The message loop can also be implemented manually using `processNextMessage()` and looping while `endpoint.isActive()`.

The endpoint enforces the protocol lifecycle: requests that arrive before `initialize` are rejected, requests after `shutdown` are rejected, and `exit` stops the loop. You still register `onInitialize`, `onShutdown` and `onExit` to do your own setup and cleanup.

### Writing a Client

A client usually launches the server process itself (see [Starting a Server Process](#starting-a-server-process)) or connects to a running one over a socket (see [Using Sockets](#using-sockets)). Either way it gets a stream to construct an `lsp::ClientEndpoint` from.

Unlike a server, a client typically runs the message loop on a background thread so the main thread can send requests and block on their results:

```cpp
#include <thread>
#include <lsp/client_endpoint.h>
#include <lsp/process.h>

int main(int argc, char** argv)
{
  auto process  = lsp::Process::start("/path/to/server/executable");
  auto endpoint = lsp::ClientEndpoint(process.stdIO());

  // Run the message loop on a separate thread so we can call RequestResult::get on the main thread
  auto messageThread = std::thread([&]{ endpoint.runMessageLoop(); });

  // The first message must be 'initialize', followed by the 'initialized' notification
  auto params = lsp::InitializeParams();
  params.processId = lsp::Process::currentProcessId();
  params.rootUri   = lsp::Uri::fileUriFromPath(".");

  // Call the 'initialize' request. get() waits until the response is returned
  const auto result = endpoint.initialize(params).get();

  // Check server capabilities from the initialize result here...

  // Send the 'initialized' notification to tell the server that the client is ready
  endpoint.initialized({});

  // Send a request and wait for the response
  auto hoverParams = lsp::HoverParams();
  hoverParams.textDocument.uri = lsp::Uri::fileUriFromPath("example.txt");
  hoverParams.position         = {0, 0};
  const auto hover = endpoint.textDocumentHover(hoverParams).get();

  // Do something with the hover result...

  // Shut the server down by sending the 'shutdown' request followed by the 'exit' notification
  (void)endpoint.shutdown().get();
  endpoint.exit();

  messageThread.join();
}
```

The endpoint throws `std::logic_error` if any message other than `initialize` is sent first, or if anything other than `exit` is sent after `shutdown`.

Outgoing requests return an `lsp::RequestResult`; calling `.get()` on it blocks until the response arrives, so never do that from the message-loop thread. See [Sending Requests and Notifications](#sending-requests-and-notifications) for the details, and [Handling Incoming Messages](#handling-incoming-messages) for reacting to server-to-client messages such as `window/logMessage`.

### Sending Requests and Notifications

Both endpoints have one method per message they can send, named like the `on...` handlers but without the prefix (`textDocument/publishDiagnostics` becomes `textDocumentPublishDiagnostics`). These methods are safe to call from any thread, including while another thread runs the message loop.

**Notifications** take the parameters (if any) and return nothing:

```cpp
serverEndpoint.textDocumentPublishDiagnostics({
  .uri         = uri,
  .diagnostics = diagnostics,
});
```

**Requests** come in two forms.

The first returns an `lsp::RequestResult`. Call `get()` to wait for and retrieve the result. It rethrows an `lsp::ResponseError` if the server responded with an error. `get()` blocks, so don't call it on the message-loop thread. `RequestResult` also has `wait(timeoutMs)` and `requestId()`. The request id can be used with `$/cancelRequest`, to cancel longer running tasks on the server, for example.

```cpp
try
{
  const auto result = clientEndpoint.textDocumentHover(params).get();
}
catch(const lsp::ResponseError& e)
{
  // The server responded with an error
}
```

The second form takes a result callback and an optional error callback and returns the `lsp::MessageId` of the request. The callbacks run on the message-loop thread once the response arrives, which makes this form safe to use from inside a message handler, where `get()` would deadlock:

```cpp
clientEndpoint.textDocumentHover(params,
  [](lsp::TextDocumentHoverResult&& result)
  {
    // Success
  },
  [](const lsp::ResponseError& error)
  {
    // Failure (optional, omit to ignore errors)
  });
```

### Handling Incoming Messages

Every message an endpoint can receive has an `on<MessageName>` method that registers a handler for it. This is how a client reacts to server-initiated messages, and a server to client requests. Handlers run on the same thread as the message-loop. Registering a handler for a message that already has one replaces it.

A **request** handler receives the parameters by rvalue reference (omitted when the message has none) and returns the result:

```cpp
clientEndpoint.onWindowShowMessageRequest(
  [](lsp::ShowMessageRequestParams&& params) -> lsp::WindowShowMessageRequestResult
  {
    if(params.actions && !params.actions->empty())
      return params.actions->front();

    return nullptr;
  });
```

A **notification** handler takes the parameters the same way and returns `void`:

```cpp
clientEndpoint
  .onWindowLogMessage(
    [](lsp::LogMessageParams&& params)
    {
      std::clog << params.message << '\n';
    })
  .onTextDocumentPublishDiagnostics(
    [](lsp::PublishDiagnosticsParams&& params)
    {
      // Update the diagnostics for params.uri
    });
```

The parameters are passed by rvalue reference so handlers can move data out of them. This prevents potentially expensive copies such as the full content of a text document. A handler that doesn't need to move anything out can take the parameters by const reference instead.

### Returning Errors

To fail a request, throw `lsp::RequestError` from its handler and the framework turns it into an LSP error response:

```cpp
#include <lsp/error.h>

serverEndpoint.onTextDocumentHover(
  [](lsp::HoverParams&& params) -> lsp::TextDocumentHoverResult
  {
    if(!documentIsOpen(params.textDocument.uri))
      throw lsp::RequestError(lsp::MessageError::InvalidParams, "Document is not open");

    return /* ... */;
  });
```

The error code is one of the constants in `lsp::MessageError` (`InvalidParams`, `InternalError`, `RequestFailed`, `RequestCancelled`, ...). Any other exception thrown from a handler is sent as an `InternalError`.

On the sending side the error arrives as an `lsp::ResponseError` with `code()`, `message()` and `data()`. `RequestResult::get()` rethrows it, and the callback form passes it to the error callback.

### Asynchronous Handlers

A request handler that returns `std::future<Result>` instead of the result directly is still invoked synchronously on the message-loop thread, but the framework then resolves the returned future (calls `get()` on it) on a worker thread, so long-running work doesn't block the message loop. The framework only spawns worker threads if at least one asynchronous handler is registered.

```cpp
serverEndpoint.onTextDocumentDefinition(
  [](lsp::DefinitionParams&& params) -> std::future<lsp::TextDocumentDefinitionResult>
  {
    return std::async(std::launch::deferred,
      [params = std::move(params)]() -> lsp::TextDocumentDefinitionResult
      {
        // Runs on a worker thread
        return computeDefinition(params);
      });
  });
```

`std::launch::deferred` makes the task body run when the framework calls `get()` on the worker thread, so no thread is created beyond the framework's pool. Throwing `lsp::RequestError` from the task still produces an error response.

Notification handlers can be asynchronous the same way by returning `std::future<void>`.

### Custom Messages

Messages that aren't part of the meta model, such as proprietary `$/` extensions, can still be received and sent using `endpoint.messageHandler()`. The `lsp::GenericRequest` and `lsp::GenericNotification` message types carry `lsp::json::Value` parameters and results instead of generated structs.

```cpp
auto& handler = clientEndpoint.messageHandler();

// Send a custom request and wait for the response
auto params = lsp::json::Object();
params["path"] = "example.txt";

const lsp::json::Value result =
  handler.sendCustomRequest<lsp::GenericRequest>("$/myExtension/stat", params).get();

// Handle a custom request
handler.onCustom<lsp::GenericRequest>("$/myExtension/stat",
  [](lsp::json::Value&& params) -> lsp::json::Value
  {
    auto response = lsp::json::Object();
    response["size"] = 42;
    return response;
  });
```

Use `lsp::GenericRequestNoParams` / `lsp::GenericNotificationNoParams` for messages without parameters, and `sendCustomNotification` to send a notification.

## Starting a Server Process

When implementing an LSP client, it is usually responsible for creating the server process. This can be done with the `lsp::Process` class. Its `stdIO()` method returns an `lsp::io::Stream` for the process's standard input and output that an endpoint can be constructed from.

```cpp
#include <lsp/process.h>
#include <lsp/client_endpoint.h>

auto process  = lsp::Process::start("/usr/bin/clangd", {/*args*/});
auto endpoint = lsp::ClientEndpoint(process.stdIO());
```

## Using Sockets

Sockets are a typical method of communication between language servers and clients, besides stdio. The framework supports connecting to an existing address and port, as well as creating a server and listening for incoming connections.

Clients can use `lsp::io::Socket::connect` to create a new socket for a given address/port combination and use it to initialize a connection:

```cpp
#include <lsp/io/socket.h>

unsigned short port = 12345;
auto socket   = lsp::io::Socket::connect(lsp::io::Socket::Localhost, port);
auto endpoint = lsp::ClientEndpoint(socket);
```

Servers need to listen for incoming socket connections. This is done by creating an `lsp::io::SocketListener` and calling its `accept` method in a loop. It waits until a new socket connection is made and returns an `lsp::io::Socket`. Since multiple connections can be accepted at once, it is possible for a single server executable to communicate with multiple clients. The following example creates a socket server which is listening for incoming connections. If one is made, a new thread is spawned, which uses the socket to create and run a new server instance for that connection:

```cpp
#include <lsp/io/socket.h>

// 0 means use an automatically assigned port. Get it with SocketListener::port.
auto listener = lsp::io::SocketListener(0);

while(listener.isOpen())
{
  auto socket = listener.accept();

  if(!socket.isOpen())
    break;

  // Start a server instance in a new thread and detach it,
  // so the main thread can keep listening for incoming connections
  std::thread([socket = std::move(socket)]() mutable
  {
    auto endpoint = lsp::ServerEndpoint(socket);
    // Do something with the endpoint...
  }).detach();
}
```

## Examples

Full server and client implementations are in [`examples/`](./examples/).

The client either launches the server itself and talks to it over stdio, or connects to a running one over a socket:

```sh
./example_client --exe=<server_executable> [server args]
./example_client --port=<port>
```

The server reads from stdin by default, or listens for socket connections with `example_server --port=<port>`.

## License

This project is licensed under the [MIT License](LICENSE).

Third-party material included in this repository (the LSP meta model) is
covered by a separate license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
