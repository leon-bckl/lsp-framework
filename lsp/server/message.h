#pragma once

#include <lsp/json/cppobj.h>

namespace lsp::server{

enum class ErrorCode{
	ServerNotInitialized = -32002,
	UnknownErrorCode     = -32001,
	RequestFailed        = -32803,
	ServerCancelled      = -32802,
	ContentModified      = -32801,
	RequestCancelled     = -32800
};

struct ClientInfo{
	JSON_OBJ(ClientInfo)
	JSON_PROPERTY(std::string,                 name)
	JSON_PROPERTY(json::Optional<std::string>, version)
};

struct ChangeAnnotationSupport{
	JSON_OBJ(ChangeAnnotationSupport)
	JSON_PROPERTY(json::Optional<bool>, groupsOnLabel)
};

struct DidChangeConfigurationClientCapabilities{
	JSON_OBJ(DidChangeConfigurationClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct DidChangeWatchedFilesClientCapabilities{
	JSON_OBJ(DidChangeWatchedFilesClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, relativePatternSupport)
};

struct SymbolKind{
	JSON_OBJ(SymbolKind)
	JSON_PROPERTY(json::Optional<std::vector<unsigned int>>, valueSet) // ENUM: SymbolKind
};

struct WorkspaceTagSupport{
	JSON_OBJ(WorkspaceTagSupport)
	JSON_PROPERTY(std::vector<unsigned int>, valueSet) // ENUM: SymbolTag
};

struct ResolveSupport{
	JSON_OBJ(ResolveSupport)
	JSON_PROPERTY(std::vector<std::string>, properties)
};

struct WorkspaceSymbolClientCapabilities{
	JSON_OBJ(WorkspaceSymbolClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                dynamicRegistration)
	JSON_PROPERTY(json::Optional<SymbolKind>,          symbolKind)
	JSON_PROPERTY(json::Optional<WorkspaceTagSupport>, tagSupport)
	JSON_PROPERTY(json::Optional<ResolveSupport>,      resolveSupport)
};

struct ExecuteCommandClientCapabilities{
	JSON_OBJ(ExecuteCommandClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct SemanticTokensWorkspaceClientCapabilities{
	JSON_OBJ(SemanticTokensWorkspaceClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, refreshSupport)
};

struct CodeLensClientCapabilities{
	JSON_OBJ(CodeLensClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, refreshSupport)
};

struct WorkspaceFileOperations{
	JSON_OBJ(WorkspaceFileOperations)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, didCreate)
	JSON_PROPERTY(json::Optional<bool>, willCreate)
	JSON_PROPERTY(json::Optional<bool>, didRename)
	JSON_PROPERTY(json::Optional<bool>, willRename)
	JSON_PROPERTY(json::Optional<bool>, didDelete)
	JSON_PROPERTY(json::Optional<bool>, willDelete)
};

struct InlineValueWorkspaceClientCapabilities{
	JSON_OBJ(InlineValueWorkspaceClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, refreshSupport)
};

struct InlayHintWorkspaceClientCapabilities{
	JSON_OBJ(InlayHintWorkspaceClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, refreshSupport)
};

struct DiagnosticsWorkspaceClientCapabilities{
	JSON_OBJ(DiagnosticsWorkspaceClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, refreshSupport)
};

struct WorkspaceEditClientCapabilities{
	JSON_OBJ(WorkspaceEditClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                                      documentChanges)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>,                  resourceOperations) // ENUM: ResourceOperationKind
	JSON_PROPERTY(json::Optional<std::string>,                               failureHandling) // ENUM: FailureHandlingKind
	JSON_PROPERTY(json::Optional<bool>,                                      normalizesLineEndings)
	JSON_PROPERTY(json::Optional<ChangeAnnotationSupport>,                   changeAnnotationSupport)
};

struct WorkspaceClientCapabilities{
	JSON_OBJ(WorkspaceClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                                      applyEdit)
	JSON_PROPERTY(json::Optional<WorkspaceEditClientCapabilities>,           workspaceEdit)
	JSON_PROPERTY(json::Optional<DidChangeConfigurationClientCapabilities>,  didChangeConfiguration)
	JSON_PROPERTY(json::Optional<DidChangeWatchedFilesClientCapabilities>,   didChangeWatchedFiles)
	JSON_PROPERTY(json::Optional<WorkspaceSymbolClientCapabilities>,         symbol)
	JSON_PROPERTY(json::Optional<ExecuteCommandClientCapabilities>,          executeCommand)
	JSON_PROPERTY(json::Optional<bool>,                                      workspaceFolders)
	JSON_PROPERTY(json::Optional<bool>,                                      configuration)
	JSON_PROPERTY(json::Optional<SemanticTokensWorkspaceClientCapabilities>, semanticTokens)
	JSON_PROPERTY(json::Optional<CodeLensClientCapabilities>,                codeLens)
	JSON_PROPERTY(json::Optional<WorkspaceFileOperations>,                   fileOperations)
	JSON_PROPERTY(json::Optional<InlineValueWorkspaceClientCapabilities>,    inlineValue)
	JSON_PROPERTY(json::Optional<InlayHintWorkspaceClientCapabilities>,      inlayHint)
	JSON_PROPERTY(json::Optional<DiagnosticsWorkspaceClientCapabilities>,    diagnostics)
};

struct TextDocumentSyncClientCapabilities{
	JSON_OBJ(TextDocumentSyncClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, willSave)
	JSON_PROPERTY(json::Optional<bool>, willSaveWaitUntil)
	JSON_PROPERTY(json::Optional<bool>, didSave)
};

struct TagSupport{
	JSON_OBJ(TagSupport)
	JSON_PROPERTY(std::vector<unsigned int>, valueSet) // ENUM: CompletionItemTag
};

struct InsertTextModeSupport{
	JSON_OBJ(InsertTextModeSupport)
	JSON_PROPERTY(std::vector<unsigned int>, valueSet) // ENUM: InsertTextMode
};

struct CompletionItem{
	JSON_OBJ(CompletionItem)
	JSON_PROPERTY(json::Optional<bool>,                     snippetSupport)
	JSON_PROPERTY(json::Optional<bool>,                     commitCharactersSupport)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>, documentationFormat) // ENUM: MarkupKind
	JSON_PROPERTY(json::Optional<bool>,                     deprecatedSupport)
	JSON_PROPERTY(json::Optional<bool>,                     preselectSupport)
	JSON_PROPERTY(json::Optional<TagSupport>,               tagSupport)
	JSON_PROPERTY(json::Optional<bool>,                     insertReplaceSupport)
	JSON_PROPERTY(json::Optional<ResolveSupport>,           resolveSupport)
	JSON_PROPERTY(json::Optional<InsertTextModeSupport>,    insertTextModeSupport)
	JSON_PROPERTY(json::Optional<bool>,                     labelDetailsSupport)
};

struct CompletionItemKind{
	JSON_OBJ(CompletionItemKind)
	JSON_PROPERTY(json::Optional<std::vector<unsigned int>>, valueSet) // ENUM: CopmletionItemKind
};

struct CompletionList{
	JSON_OBJ(CompletionList)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>, itemDefaults)
};

struct CompletionClientCapabilities{
	JSON_OBJ(CompletionClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,               dynamicRegistration)
	JSON_PROPERTY(json::Optional<CompletionItem>,     completionItem)
	JSON_PROPERTY(json::Optional<CompletionItemKind>, completionItemKind)
	JSON_PROPERTY(json::Optional<bool>,               contextSupport)
	JSON_PROPERTY(json::Optional<unsigned int>,       insertTextMode) // ENUM: InsertTextMode
	JSON_PROPERTY(json::Optional<CompletionList>,     completionList)
};

struct HoverClientCapabilities{
	JSON_OBJ(HoverClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                     dynamicRegistration)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>, contentFormat) // ENUM: MarkupKind
};

struct ParameterInformation{
	JSON_OBJ(ParameterInformation)
	JSON_PROPERTY(json::Optional<bool>, labelOffsetSupport)
};

struct SignatureInformation{
	JSON_OBJ(SignatureInformation)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>, documentationFormat) // ENUM: MarkupKind
	JSON_PROPERTY(json::Optional<ParameterInformation>,     parameterInformation)
	JSON_PROPERTY(json::Optional<bool>,                     activeParameterSupport)
};

struct SignatureHelpClientCapabilities{
	JSON_OBJ(SignatureHelpClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                 dynamicRegistration)
	JSON_PROPERTY(json::Optional<SignatureInformation>, signatureInformation)
	JSON_PROPERTY(json::Optional<bool>,                 contextSupport)
};

struct DeclDefClientCapabilities{
	JSON_OBJ(DeclDefClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, linkSupport)
};

struct ReferenceClientCapabilities{
	JSON_OBJ(ReferenceClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct DocumentHighlightClientCapabilities{
	JSON_OBJ(DocumentHighlightClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct DocumentSymbolClientCapabilties{
	JSON_OBJ(DocumentSymbolClientCapabilties)
	JSON_PROPERTY(json::Optional<bool>,       dynamicRegistration)
	JSON_PROPERTY(json::Optional<SymbolKind>, symbolKind)
	JSON_PROPERTY(json::Optional<bool>,       hierarchicalDocumentSymbolSupport)
	JSON_PROPERTY(json::Optional<TagSupport>, tagSupport)
	JSON_PROPERTY(json::Optional<bool>,       labelSupport)
};

struct CodeActionKind{
	JSON_OBJ(CodeActionKind)
	JSON_PROPERTY(std::vector<std::string>, valueSet) // ENUM: CodeActionKind
};

struct CodeActionLiteralSupport{
	JSON_OBJ(CodeActionLiteralSupport)
	JSON_PROPERTY(json::Optional<CodeActionKind>, codeActionKind)
};

struct CodeActionClientCapabilities{
	JSON_OBJ(CodeActionClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                     dynamicRegistration)
	JSON_PROPERTY(json::Optional<CodeActionLiteralSupport>, codeActionLiteralSupport)
	JSON_PROPERTY(json::Optional<bool>,                     isPreferredSupport)
	JSON_PROPERTY(json::Optional<bool>,                     disabledSupport)
	JSON_PROPERTY(json::Optional<bool>,                     dataSupport)
	JSON_PROPERTY(json::Optional<ResolveSupport>,           resolveSupport)
	JSON_PROPERTY(json::Optional<bool>,                     honorsChangeAnnotations)
};

struct DocumentLinkClientCapabilities{
	JSON_OBJ(DocumentLinkClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, tooltipSupport)
};

struct DocumentColorClientCapabilities{
	JSON_OBJ(DocumentColorClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct DocumentFormattingClientCapabilities{
	JSON_OBJ(DocumentFormattingClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct RenameClientCapabilities{
	JSON_OBJ(RenameClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,         dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>,         prepareSupport)
	JSON_PROPERTY(json::Optional<unsigned int>, prepareSupportDefaultBehavior) // ENUM: PrepareSupportDefaultBehavior
	JSON_PROPERTY(json::Optional<bool>,         honorsChangeAnnotations)
};

struct DiagnosticsTag{
	JSON_OBJ(DiagnosticsTag)
	JSON_PROPERTY(std::vector<unsigned int>, valueSet) // ENUM: DiagnosticsTag
};

struct PublishDiagnosticsClientCapabilities{
	JSON_OBJ(PublishDiagnosticsClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,           relatedInformation)
	JSON_PROPERTY(json::Optional<DiagnosticsTag>, tagSupport)
	JSON_PROPERTY(json::Optional<bool>,           versionSupport)
	JSON_PROPERTY(json::Optional<bool>,           codeDescriptionSupport)
	JSON_PROPERTY(json::Optional<bool>,           dataSupport)
};

struct FoldingRangeKind{
	JSON_OBJ(FoldingRangeKind)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>, valueSet) // ENUM: FoldingRangeKind
};

struct FoldingRange{
	JSON_OBJ(FoldingRange)
	JSON_PROPERTY(json::Optional<bool>, collapsedText)
};

struct FoldingRangeClientCapabilities{
	JSON_OBJ(FoldingRangeClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,             dynamicRegistration)
	JSON_PROPERTY(json::Optional<unsigned int>,     rangeLimit)
	JSON_PROPERTY(json::Optional<bool>,             lineFoldingOnly)
	JSON_PROPERTY(json::Optional<FoldingRangeKind>, foldingRangeKind)
	JSON_PROPERTY(json::Optional<FoldingRange>,     foldingRange)
};

struct SelectionRangeClientCapabilities{
	JSON_OBJ(SelectionRangeClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct LinkedEditRangeClientCapabilities{
	JSON_OBJ(LinkedEditRangeClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct CallHierarchyClientCapabilities{
	JSON_OBJ(CallHierarchyClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct SemanticTokensEmptyObject{
	JSON_OBJ(SemanticTokensEmptyObject)
};

struct SemanticTokensFull{
	JSON_OBJ(SemanticTokensFull)
	JSON_PROPERTY(json::Optional<bool>, delta)
};

struct SemanticTokensRequests{
	std::variant<bool, SemanticTokensEmptyObject> range;
	std::variant<bool, SemanticTokensFull>        full;

	void fromJson(const json::Object& json){
		auto it = json.find("range");

		if(it != json.end()){
			if(it->second.isBoolean()){
				bool value;
				json::impl::fromJson(value, it->second);
				range = value;
			}else if(it->second.isObject()){
				range = SemanticTokensEmptyObject{};
			}else{
				throw json::TypeError{"SemanticTokensRequests: Expected boolean or object"};
			}
		}

		it = json.find("full");

		if(it != json.end()){
			if(it->second.isBoolean()){
				bool value;
				json::impl::fromJson(value, it->second);
				full = value;
			}else if(it->second.isObject()){
				SemanticTokensFull value;
				json::impl::fromJson(value, it->second);
				full = value;
			}else{
				throw json::TypeError{"SemanticTokensRequests: Expected boolean or object"};
			}
		}
	}

	json::Object toJson() const{
		json::Object obj;
		obj["range"] = std::visit([](auto&& p){ return json::impl::toJson(p); }, range);
		obj["full"] = std::visit([](auto&& p){ return json::impl::toJson(p); }, full);
		return obj;
	}
};

struct SemanticTokensClientCapabilities{
	JSON_OBJ(SemanticTokensClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,     dynamicRegistration)
	JSON_PROPERTY(SemanticTokensRequests,   requests)
	JSON_PROPERTY(std::vector<std::string>, tokenTypes)
	JSON_PROPERTY(std::vector<std::string>, tokenModifiers)
	JSON_PROPERTY(std::vector<std::string>, formats) // ENUM: TokenFormat
	JSON_PROPERTY(json::Optional<bool>,     overlappingTokenSupport)
	JSON_PROPERTY(json::Optional<bool>,     multilineTokenSupport)
	JSON_PROPERTY(json::Optional<bool>,     serverCancelSupport)
	JSON_PROPERTY(json::Optional<bool>,     augmentsSyntaxTokens)
};

struct MonikerClientCapabilities{
	JSON_OBJ(MonikerClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct TypeHierarchyClientCapabilities{
	JSON_OBJ(TypeHierarchyClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct InlineValueClientCapabilities{
	JSON_OBJ(InlineValueClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
};

struct InlayHintClientCapabilities{
	JSON_OBJ(InlayHintClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,           dynamicRegistration)
	JSON_PROPERTY(json::Optional<ResolveSupport>, resolveSupport)
};

struct DiagnosticClientCapabilities{
	JSON_OBJ(DiagnosticClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, relatedDocumentSupport)
};

struct TextDocumentClientCapabilities{
	JSON_OBJ(TextDocumentClientCapabilities)
	JSON_PROPERTY(json::Optional<TextDocumentSyncClientCapabilities>,   synchronization)
	JSON_PROPERTY(json::Optional<CompletionClientCapabilities>,         completion)
	JSON_PROPERTY(json::Optional<HoverClientCapabilities>,              hover)
	JSON_PROPERTY(json::Optional<SignatureHelpClientCapabilities>,      signatureHelp)
	JSON_PROPERTY(json::Optional<DeclDefClientCapabilities>,            declaration)
	JSON_PROPERTY(json::Optional<DeclDefClientCapabilities>,            definition)
	JSON_PROPERTY(json::Optional<DeclDefClientCapabilities>,            typeDefinition)
	JSON_PROPERTY(json::Optional<DeclDefClientCapabilities>,            implementation)
	JSON_PROPERTY(json::Optional<ReferenceClientCapabilities>,          references)
	JSON_PROPERTY(json::Optional<DocumentHighlightClientCapabilities>,  documentHighlight)
	JSON_PROPERTY(json::Optional<DocumentSymbolClientCapabilties>,      documentSymbol)
	JSON_PROPERTY(json::Optional<CodeActionClientCapabilities>,         codeAction)
	JSON_PROPERTY(json::Optional<CodeLensClientCapabilities>,           codeLens)
	JSON_PROPERTY(json::Optional<DocumentLinkClientCapabilities>,       documentLink)
	JSON_PROPERTY(json::Optional<DocumentColorClientCapabilities>,      colorProvider)
	JSON_PROPERTY(json::Optional<DocumentFormattingClientCapabilities>, formatting)
	JSON_PROPERTY(json::Optional<DocumentFormattingClientCapabilities>, rangeFormatting)
	JSON_PROPERTY(json::Optional<DocumentFormattingClientCapabilities>, onTypeFormatting)
	JSON_PROPERTY(json::Optional<RenameClientCapabilities>,             rename)
	JSON_PROPERTY(json::Optional<PublishDiagnosticsClientCapabilities>, publishDiagnostics)
	JSON_PROPERTY(json::Optional<FoldingRangeClientCapabilities>,       foldingRange)
	JSON_PROPERTY(json::Optional<SelectionRangeClientCapabilities>,     selectionRange)
	JSON_PROPERTY(json::Optional<LinkedEditRangeClientCapabilities>,    linkedEditingRange)
	JSON_PROPERTY(json::Optional<CallHierarchyClientCapabilities>,      callHierarchy)
	JSON_PROPERTY(json::Optional<SemanticTokensClientCapabilities>,     semanticTokens)
	JSON_PROPERTY(json::Optional<MonikerClientCapabilities>,            moniker)
	JSON_PROPERTY(json::Optional<TypeHierarchyClientCapabilities>,      typeHierarchy)
	JSON_PROPERTY(json::Optional<InlineValueClientCapabilities>,        inlineValue)
	JSON_PROPERTY(json::Optional<InlayHintClientCapabilities>,          inlayHint)
	JSON_PROPERTY(json::Optional<DiagnosticClientCapabilities>,         diagnostic)
};

struct NotebookDocumentSyncClientCapabilities{
	JSON_OBJ(NotebookDocumentSyncClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>, dynamicRegistration)
	JSON_PROPERTY(json::Optional<bool>, executionSummarySupport)
};

struct NotebookDocumentClientCapabilities{
	JSON_OBJ(NotebookDocumentClientCapabilities)
	JSON_PROPERTY(NotebookDocumentSyncClientCapabilities, synchronization)
};

struct MessageActionItem{
	JSON_OBJ(MessageActionItem)
	JSON_PROPERTY(json::Optional<bool>, additionalPropertiesSupport)
};

struct ShowMessageRequestClientCapabilities{
	JSON_OBJ(ShowMessageRequestClientCapabilities)
	JSON_PROPERTY(json::Optional<MessageActionItem>, messageActionItem)
};

struct ShowDocumentClientCapabilities{
	JSON_OBJ(ShowDocumentClientCapabilities)
	JSON_PROPERTY(bool, support)
};

struct WindowClientCapabilities{
	JSON_OBJ(WindowClientCapabilities)
	JSON_PROPERTY(json::Optional<bool>,                                 workDoneProgress)
	JSON_PROPERTY(json::Optional<ShowMessageRequestClientCapabilities>, showMessage)
	JSON_PROPERTY(json::Optional<ShowDocumentClientCapabilities>,       showDocument)
};

struct StaleRequestSupport{
	JSON_OBJ(StaleRequestSupport)
	JSON_PROPERTY(bool, cancel)
	JSON_PROPERTY(std::vector<std::string>, retryOnContentModified)
};

struct RegularExpressionsClientCapabilities{
	JSON_OBJ(RegularExpressionsClientCapabilities)
	JSON_PROPERTY(std::string,                 engine)
	JSON_PROPERTY(json::Optional<std::string>, version)
};

struct MarkdownClientCapabilities{
	JSON_OBJ(MarkdownClientCapabilities)
	JSON_PROPERTY(std::string,                              parser)
	JSON_PROPERTY(json::Optional<std::string>,              version)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>, allowedTags)
};

struct GeneralClientCapabilities{
	JSON_OBJ(GeneralClientCapabilities)
	JSON_PROPERTY(json::Optional<StaleRequestSupport>,                  staleRequestSupport)
	JSON_PROPERTY(json::Optional<RegularExpressionsClientCapabilities>, regularExpressions)
	JSON_PROPERTY(json::Optional<MarkdownClientCapabilities>,           markdown)
	JSON_PROPERTY(json::Optional<std::vector<std::string>>,             positionEncodings) // ENUM: PositionEncodingKind
};

struct ClientCapapbilities{
	JSON_OBJ(ClientCapapbilities)
	JSON_PROPERTY(json::Optional<WorkspaceClientCapabilities>,        workspace)
	JSON_PROPERTY(json::Optional<TextDocumentClientCapabilities>,     textDocument)
	JSON_PROPERTY(json::Optional<NotebookDocumentClientCapabilities>, notebookDocument)
	JSON_PROPERTY(json::Optional<WindowClientCapabilities>,           window)
	JSON_PROPERTY(json::Optional<GeneralClientCapabilities>,          general)
	JSON_PROPERTY(json::Optional<json::Value>,                        experimental)
};

struct WorkspaceFolder{
	JSON_OBJ(WorkspaceFolder)
	JSON_PROPERTY(std::string, uri)
	JSON_PROPERTY(std::string, name)
};

struct InitializeParams{
	JSON_OBJ(InitializeParams)
	JSON_PROPERTY(json::Nullable<int>,                                  processId)
	JSON_PROPERTY(json::Optional<ClientInfo>,                           clientInfo)
	JSON_PROPERTY(json::Optional<std::string>,                          locale)
	JSON_PROPERTY(json::OptionalNullable<std::string>,                  rootPath)
	JSON_PROPERTY(json::Nullable<std::string>,                          rootUri)
	JSON_PROPERTY(json::Optional<json::Value>,                          initializationOptions)
	JSON_PROPERTY(ClientCapapbilities,                                  capabilities)
	JSON_PROPERTY(json::Optional<std::string>,                          trace) // ENUM: TraceValue
	JSON_PROPERTY(json::OptionalNullable<std::vector<WorkspaceFolder>>, workspaceFolders)
};

}
