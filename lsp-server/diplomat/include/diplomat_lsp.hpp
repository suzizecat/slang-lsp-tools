#pragma once

#include "lsp.hpp"
#include "index_core.hpp"
#include "diagnostic_client.hpp"
#include "diplomat_lsp_ws_settings.hpp"
#include "diplomat_document_cache.hpp"
// #include "diplomat_index.hpp"


#include "types/structs/Location.hpp"
#include "types/structs/WorkspaceFolder.hpp"
#include "types/structs/ClientCapabilities.hpp" 


#include "slang/ast/Compilation.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "visitor_module_bb.hpp"

#include <iostream>
#include <unordered_set>
#include <memory>
#include <filesystem>
#include <thread>

#include <stop_token>


using json = nlohmann::json;


namespace diplomat::app 
{

class DiplomatLSP : public lsp::BaseLSP
{
    protected:

        void _h_didChangeWorkspaceFolders(json params, std::stop_token tk);
        void _h_didSaveTextDocument(lsp::types::DidSaveTextDocumentParams params, std::stop_token tk);
        void _h_didOpenTextDocument(json params, std::stop_token tk);
        void _h_didCloseTextDocument(lsp::types::DidCloseTextDocumentParams params, std::stop_token tk);
        json _h_completion(lsp::types::CompletionParams params, std::stop_token tk);
        json _h_formatting(lsp::types::DocumentFormattingParams params, std::stop_token tk);
        json _h_gotoDefinition(lsp::types::DefinitionParams params, std::stop_token tk);
        json _h_references(json params, std::stop_token tk);
        json _h_rename(json params, std::stop_token tk);
        void _h_exit(json params, std::stop_token tk);
        json _h_initialize(lsp::types::InitializeParams params, std::stop_token tk);
        void _h_initialized(json params, std::stop_token tk);
        void _h_setTrace(json params, std::stop_token tk);
        json _h_shutdown(json params, std::stop_token tk);

        void _h_set_project(lsp::types::DiplomatProject params, std::stop_token tk);

        void _h_push_config(DiplomatLSPWorkspaceSettings params, std::stop_token tk);
        json _h_pull_config(json params, std::stop_token tk);
        // void _h_get_configuration(json& params, std::stop_token tk);
        // void _h_get_configuration_on_init(json& params, std::stop_token tk);
        // void _h_update_configuration(json& params, std::stop_token tk);
        const std::vector< const ModuleBlackBox*>  _h_get_file_bb(std::string params, std::stop_token tk);
        std::vector<lsp::types::HDLModule> _h_get_modules(json _, std::stop_token tk);
        const std::vector< const ModuleBlackBox*> _h_get_module_bbox(lsp::types::HDLModule params, std::stop_token tk);
        void _h_set_top_module(std::string params, std::stop_token tk);
        std::vector<std::string> _h_project_tree_from_module(lsp::types::HDLModule params, std::stop_token tk);
        void _h_ignore(std::vector<std::string> params, std::stop_token tk);
        void _h_add_to_include(json params, std::stop_token tk);
        void _h_force_clear_index(json params, std::stop_token tk);

        std::map<std::string,std::optional<lsp::types::Location>> _h_resolve_hier_path(std::vector<std::string> params, std::stop_token tk);
        json _h_get_design_hierarchy(json params, std::stop_token tk);
        std::map<std::string,std::vector<lsp::types::Range>> _h_list_symbols(std::string params, std::stop_token tk);

        void _bind_methods();

        void _clear_diagnostics();
        void _cleanup_diagnostics();

        void _emit_diagnostics();
        void _erase_diagnostics();

        // SVDocument* _read_document(std::filesystem::path path);

        std::unique_ptr<slang::SourceManager> _sm;

        diplomat::cache::DiplomatDocumentCache _cache;
       
        std::vector< std::filesystem::path> _root_dirs;
        std::unordered_set< std::filesystem::path> _excluded_paths;
        
        std::unordered_set<std::string> _accepted_extensions;
        std::filesystem::path _settings_path;
        DiplomatLSPWorkspaceSettings _settings;

        /** 
        * May require further work, represents "includes" for the
        * current project 
        */
        std::vector<std::string> _included_folders;

        std::shared_ptr<LSPDiagnosticClient> _diagnostic_client;

        std::unique_ptr<diplomat::index::IndexCore> _index;


        bool _project_file_tree_valid;

        bool _watch_client_pid;
        bool _broken_index_emitted;
        std::jthread _pid_watcher;

        std::unique_ptr<slang::ast::Compilation> _compilation;
        std::unique_ptr<slang::SourceLibrary> _default_source_lib;
        

        lsp::types::Location _slang_to_lsp_location(const slang::SourceRange& sr) const;
        // Needs line-col -> offset which is a bit tricky to do
        // Needs filepath -> BufferID() which is tricky.
        // slang::SourceRange _lsp_to_slang_location(const lsp::types::Location& loc) const;

        static diplomat::index::IndexLocation _lsp_to_index_location(const lsp::types::TextDocumentPositionParams& loc);
        lsp::types::Location _index_range_to_lsp(const diplomat::index::IndexRange& loc) const;

        void _add_workspace_folders(const std::vector<lsp::types::WorkspaceFolder>& to_add);
        void _remove_workspace_folders(const std::vector<lsp::types::WorkspaceFolder>& to_rm);

        void _read_workspace_modules();
        void _read_filetree_modules();
        void _compile();
                
        void _save_client_uri(const std::string& client_uri);

        bool _assert_index(bool always_throw = false);

        void _compute_project_tree(bool keep_tree = true);
        void _clear_project_tree();
        void _add_module_to_project_tree(const std::string& mod);

        // const SVDocument* _document_from_module(const std::string& module) const;
        const ModuleBlackBox* _bb_from_module(const std::string& module) const;
        const std::vector<ModuleBlackBox> _bb_from_file(const std::string& fpath) const;

    public:
        explicit DiplomatLSP(std::istream& is = std::cin, std::ostream& os = std::cout, bool watch_client_pid = true);

        slang::ast::Compilation* get_compilation();
        // inline const std::unordered_map<std::filesystem::path, std::unique_ptr<SVDocument>>& get_documents() const {return _documents;};
        
        //void read_config(std::filesystem::path& filepath);
        void hello(json params);
        void dump_index(json params, std::stop_token tk);

        void set_top_level(const std::string& new_top);

        inline void set_watch_client_pid(bool new_value) {_watch_client_pid = new_value;};

};

}
