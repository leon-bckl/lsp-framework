# lsp-framework

This is an implementation of the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) in C++. It can be used to implement both servers and clients that communicate using the LSP.

## Dependencies

There aren't any external dependencies except for `cmake` and a compiler that supports C++20.

## Usage

The project is built as a static library. LSP type definitions, messages and serialization boilerplate are generated during the build from the official [meta model](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/metaModel/metaModel.json).  
  
Here's a short example on how to handle and send requests:

```cpp
#include <lsp/messages.h> /* Generated message definitions */
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

...

lsp::Connection connection{lsp::io::standardInput(), lsp::io::standardOutput()};
lsp::MessageHandler messageHandler{connection};
bool running = true;

messageHandler.requestHandler()
.add<lsp::requests::Initialize>([](const jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params)
{
  lsp::requests::Initialize::Result result;
  // Initialize the result and return it or throw an lsp::RequestError if there was a problem
  // Alternatively do processing asynchronously and return a std::future here
  return result;
})
.add<lsp::notifications::Exit>([&running]()
{
  running = false;
});

while(running)
  messageHandler.processIncomingMessages();

...
```

```cpp
// The sendRequest method returns a std::future for the result type of the message.
// Be careful not to call std::future::wait on the same thread that calls
// MessageHandler::processIncomingMessages since it would block.

auto result = messageHandler.messageDispatcher().sendRequest<lsp::requests::TextDocument_Diagnostic>(
    lsp::requests::TextDocument_Diagnostic::Params{...}
  );

```

## License

This project is licensed under the [MIT License](LICENSE).
