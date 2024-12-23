// Copyright 2017-2018 ccls Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "lsp.hh"
#include "query.hh"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ccls {
struct SemaManager;
struct VFS;
struct Project;
struct WorkingFile;
struct WorkingFiles;

namespace pipeline {
void reply(const RequestId &id, const std::function<void(JsonWriter &)> &fn);
void replyError(const RequestId &id,
                const std::function<void(JsonWriter &)> &fn);
} // namespace pipeline

struct CodeActionParam {
  TextDocumentIdentifier textDocument;
  lsRange range;
  struct Context {
    std::vector<Diagnostic> diagnostics;
  } context;
};
struct EmptyParam {};
struct DidOpenTextDocumentParam {
  TextDocumentItem textDocument;
};
struct RenameParam {
  TextDocumentIdentifier textDocument;
  Position position;
  std::string newName;
};
struct SemanticTokensRangeParams {
  TextDocumentIdentifier textDocument;
  lsRange range;
};
REFLECT_STRUCT(SemanticTokensRangeParams, textDocument, range);
struct TextDocumentParam {
  TextDocumentIdentifier textDocument;
};
struct TextDocumentPositionParam {
  TextDocumentIdentifier textDocument;
  Position position;
};
struct TextDocumentEdit {
  VersionedTextDocumentIdentifier textDocument;
  std::vector<TextEdit> edits;
};
REFLECT_STRUCT(TextDocumentEdit, textDocument, edits);
struct WorkspaceEdit {
  std::vector<TextDocumentEdit> documentChanges;
};
REFLECT_STRUCT(WorkspaceEdit, documentChanges);

struct CallHierarchyItem {
  std::string name;
  SymbolKind kind;
  std::string detail;
  DocumentUri uri;
  lsRange range;
  lsRange selectionRange;
  std::string data;
};
REFLECT_STRUCT(CallHierarchyItem, name, kind, detail, uri, range,
               selectionRange, data);

struct CallsParam {
  CallHierarchyItem item;
};

// completion
enum class CompletionTriggerKind {
  Invoked = 1,
  TriggerCharacter = 2,
  TriggerForIncompleteCompletions = 3,
};
struct CompletionContext {
  CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;
  std::optional<std::string> triggerCharacter;
};
struct CompletionParam : TextDocumentPositionParam {
  CompletionContext context;
};
enum class CompletionItemKind {
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25,
};
enum class InsertTextFormat { PlainText = 1, Snippet = 2 };
struct CompletionItem {
  std::string label;
  CompletionItemKind kind = CompletionItemKind::Text;
  std::string detail;
  std::string documentation;
  std::string sortText;
  std::string filterText;
  InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;
  TextEdit textEdit;
  std::vector<TextEdit> additionalTextEdits;

  std::vector<std::string> parameters_;
  int score_;
  unsigned priority_;
  int quote_kind_ = 0;
};

// formatting
struct FormattingOptions {
  int tabSize;
  bool insertSpaces;
};
struct DocumentFormattingParam {
  TextDocumentIdentifier textDocument;
  FormattingOptions options;
};
struct DocumentOnTypeFormattingParam {
  TextDocumentIdentifier textDocument;
  Position position;
  std::string ch;
  FormattingOptions options;
};
struct DocumentRangeFormattingParam {
  TextDocumentIdentifier textDocument;
  lsRange range;
  FormattingOptions options;
};

// workspace
enum class FileChangeType {
  Created = 1,
  Changed = 2,
  Deleted = 3,
};
struct DidChangeWatchedFilesParam {
  struct Event {
    DocumentUri uri;
    FileChangeType type;
  };
  std::vector<Event> changes;
};
struct DidChangeWorkspaceFoldersParam {
  struct Event {
    std::vector<WorkspaceFolder> added, removed;
  } event;
};
struct WorkspaceSymbolParam {
  std::string query;

  // ccls extensions
  std::vector<std::string> folders;
};
REFLECT_STRUCT(WorkspaceFolder, uri, name);

inline void reflect(JsonReader &vis, DocumentUri &v) {
  reflect(vis, v.raw_uri);
}
inline void reflect(JsonWriter &vis, DocumentUri &v) {
  reflect(vis, v.raw_uri);
}
inline void reflect(JsonReader &vis, VersionedTextDocumentIdentifier &v) {
  REFLECT_MEMBER(uri);
  REFLECT_MEMBER(version);
}
inline void reflect(JsonWriter &vis, VersionedTextDocumentIdentifier &v) {
  vis.startObject();
  REFLECT_MEMBER(uri);
  vis.key("version");
  reflect(vis, v.version);
  vis.endObject();
}

REFLECT_UNDERLYING(ErrorCode);
REFLECT_STRUCT(ResponseError, code, message);
REFLECT_STRUCT(Position, line, character);
REFLECT_STRUCT(lsRange, start, end);
REFLECT_STRUCT(Location, uri, range);
REFLECT_STRUCT(LocationLink, targetUri, targetRange, targetSelectionRange);
REFLECT_UNDERLYING_B(SymbolKind);
REFLECT_STRUCT(TextDocumentIdentifier, uri);
REFLECT_STRUCT(TextDocumentItem, uri, languageId, version, text);
REFLECT_STRUCT(TextEdit, range, newText);
REFLECT_STRUCT(WorkDoneProgress, kind, title, message, percentage);
REFLECT_STRUCT(WorkDoneProgressParam, token, value);
REFLECT_STRUCT(DiagnosticRelatedInformation, location, message);
REFLECT_STRUCT(Diagnostic, range, severity, code, source, message,
               relatedInformation);
REFLECT_STRUCT(ShowMessageParam, type, message);
REFLECT_UNDERLYING_B(LanguageId);

struct NotIndexed {
  std::string path;
};
struct MessageHandler;

struct ReplyOnce {
  MessageHandler &handler;
  RequestId id;
  template <typename Res> void operator()(Res &&result) const {
    if (id.valid())
      pipeline::reply(id, [&](JsonWriter &w) { reflect(w, result); });
  }
  void error(ErrorCode code, std::string message) const {
    ResponseError err{code, std::move(message)};
    if (id.valid())
      pipeline::replyError(id, [&](JsonWriter &w) { reflect(w, err); });
  }
  void notOpened(std::string_view path);
  void replyLocationLink(std::vector<LocationLink> &result);
};

struct MessageHandler {
  SemaManager *manager = nullptr;
  DB *db = nullptr;
  Project *project = nullptr;
  VFS *vfs = nullptr;
  WorkingFiles *wfiles = nullptr;

  llvm::StringMap<std::function<void(JsonReader &)>> method2notification;
  llvm::StringMap<std::function<void(JsonReader &, ReplyOnce &)>>
      method2request;
  bool overdue = false;

  MessageHandler();
  void run(InMessage &msg);
  QueryFile *findFile(const std::string &path, int *out_file_id = nullptr);
  std::pair<QueryFile *, WorkingFile *> findOrFail(const std::string &path,
                                                   ReplyOnce &reply,
                                                   int *out_file_id = nullptr,
                                                   bool allow_unopened = false);

private:
  void bind(const char *method, void (MessageHandler::*handler)(JsonReader &));
  template <typename Param>
  void bind(const char *method, void (MessageHandler::*handler)(Param &));
  void bind(const char *method,
            void (MessageHandler::*handler)(JsonReader &, ReplyOnce &));
  template <typename Param>
  void bind(const char *method,
            void (MessageHandler::*handler)(Param &, ReplyOnce &));

  void ccls_call(JsonReader &, ReplyOnce &);
  void ccls_fileInfo(JsonReader &, ReplyOnce &);
  void ccls_info(EmptyParam &, ReplyOnce &);
  void ccls_inheritance(JsonReader &, ReplyOnce &);
  void ccls_member(JsonReader &, ReplyOnce &);
  void ccls_navigate(JsonReader &, ReplyOnce &);
  void ccls_reload(JsonReader &);
  void ccls_vars(JsonReader &, ReplyOnce &);
  void callHierarchy_incomingCalls(CallsParam &param, ReplyOnce &);
  void callHierarchy_outgoingCalls(CallsParam &param, ReplyOnce &);
  void exit(EmptyParam &);
  void initialize(JsonReader &, ReplyOnce &);
  void initialized(EmptyParam &);
  void shutdown(EmptyParam &, ReplyOnce &);
  void textDocument_codeAction(CodeActionParam &, ReplyOnce &);
  void textDocument_codeLens(TextDocumentParam &, ReplyOnce &);
  void textDocument_completion(CompletionParam &, ReplyOnce &);
  void textDocument_declaration(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_definition(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_didChange(TextDocumentDidChangeParam &);
  void textDocument_didClose(TextDocumentParam &);
  void textDocument_didOpen(DidOpenTextDocumentParam &);
  void textDocument_didSave(TextDocumentParam &);
  void textDocument_documentHighlight(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_documentLink(TextDocumentParam &, ReplyOnce &);
  void textDocument_documentSymbol(JsonReader &, ReplyOnce &);
  void textDocument_foldingRange(TextDocumentParam &, ReplyOnce &);
  void textDocument_formatting(DocumentFormattingParam &, ReplyOnce &);
  void textDocument_hover(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_implementation(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_onTypeFormatting(DocumentOnTypeFormattingParam &,
                                     ReplyOnce &);
  void textDocument_prepareCallHierarchy(TextDocumentPositionParam &,
                                         ReplyOnce &);
  void textDocument_rangeFormatting(DocumentRangeFormattingParam &,
                                    ReplyOnce &);
  void textDocument_references(JsonReader &, ReplyOnce &);
  void textDocument_rename(RenameParam &, ReplyOnce &);
  void textDocument_signatureHelp(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_typeDefinition(TextDocumentPositionParam &, ReplyOnce &);
  void textDocument_semanticTokensFull(TextDocumentParam &, ReplyOnce &);
  void textDocument_semanticTokensRange(SemanticTokensRangeParams &, ReplyOnce &);
  void workspace_didChangeConfiguration(EmptyParam &);
  void workspace_didChangeWatchedFiles(DidChangeWatchedFilesParam &);
  void workspace_didChangeWorkspaceFolders(DidChangeWorkspaceFoldersParam &);
  void workspace_executeCommand(JsonReader &, ReplyOnce &);
  void workspace_symbol(WorkspaceSymbolParam &, ReplyOnce &);
};

void emitSkippedRanges(WorkingFile *wfile, QueryFile &file);

void emitSemanticHighlight(DB *db, WorkingFile *wfile, QueryFile &file);
} // namespace ccls
