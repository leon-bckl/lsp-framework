# lsp-framework

This is an implementation of the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) in C++. It can be used to implement both servers and clients that communicate using the LSP.

## Dependencies

There aren't any external dependencies except for `cmake` and a compiler that supports C++20.



## Usage

The project is built as a static library. LSP type definitions, messages and serialization boilerplate are generated from the official [meta model](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/metaModel/metaModel.json) during the build. This means all LSP types are proper C++ structs, so there's no need to manually read or write JSON. The framework handles serialization and deserialization automatically.  
All messages can be found in the generated `<lsp/messages.h>` header with requests inside the `lsp::requests` and notifications inside the `lsp::notifications` namespace respectively.

### Basic Setup

1. **Establish a Connection**: Use the `Connection` class to handle communication via `std::istream` and `std::ostream`. You can pass in `std::cin`/`std::cout` or a custom implementation (e.g. sockets).
2. **Create a MessageHandler**: The `MessageHandler` class combines message receiving and sending and provides a method to listen for new messages.
3. **Register Callbacks**: Use the `RequestHandler` to register callbacks for incoming requests and notifications. Request callbacks return a result which is sent back as a response.
4. **Send Requests and Notifications**: Use the `MessageDispatcher` to send outgoing requests and notifications.

### Example

```cpp
#include <lsp/messages.h> /* Generated message definitions */
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

int main()
{
   // 1: Establish a connection using standard input/output
   lsp::Connection connection{lsp::io::standardInput(), lsp::io::standardOutput()};
   
   // 2: Create a MessageHandler with the connection
   lsp::MessageHandler messageHandler{connection};
   
   bool running = true;

   // 3: Register callbacks for incoming messages
   messageHandler.requestHandler()
      // Request callbacks always have the message id as the first parameter followed by the params if there are any.
      .add<lsp::requests::Initialize>([](const lsp::jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params)
      {
         lsp::requests::Initialize::Result result;
         // Initialize the result and return it or throw an lsp::RequestError if there was a problem
         // Alternatively do processing asynchronously and return a std::future here
         return result;
      })
      // Notifications don't have an id parameter because no response is sent back for them.
      .add<lsp::notifications::Exit>([&running]()
      {
         running = false;
      });

   // 4: Start the message processing loop
   // processIncomingMessages Reads all current messages from the connection and if there are none waits until one becomes available
   while(running)
      messageHandler.processIncomingMessages();

   return 0;
}
```

### Sending Requests

```cpp
// The sendRequest method returns a struct containing the message id and a std::future for the result type of the message.
// Don't call std::future::wait on the same thread that calls MessageHandler::processIncomingMessages since it would block.
lsp::FutureResponse response = messageHandler.messageDispatcher()
   .sendRequest<lsp::requests::TextDocument_Diagnostic>(lsp::requests::TextDocument_Diagnostic::Params{ /* parameters */ });

// Alternatively you can provide callbacks for the result and error cases instead of using a future
// The callbacks are called from inside of MessageHandler::processIncomingMessages. Uncaught exceptions will abort the connection.
jsonrpc::MessageId messageId = messageHandler.messageDispatcher().sendRequest<lsp::requests::TextDocument_Diagnostic>(
	lsp::requests::TextDocument_Diagnostic::Params{ /* parameters */ },
	[](lsp::requests::TextDocument_Diagnostic::Result&& result){
		//...
	},
	[](const lsp::Error& error){
		//...
	});
```

## License

This project is licensed under the [MIT License](LICENSE).
