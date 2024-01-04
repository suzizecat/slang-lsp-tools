#pragma once

#include "lsp.hpp"
#include "lsp_errors.hpp"
#include "sv_document.hpp"
#include "diagnostic_client.hpp"
#include "diplomat_lsp_ws_settings.hpp"
#include "diplomat_index.hpp"

#include "nlohmann/json.hpp"

#include "types/structs/ServerCapabilities.hpp"
#include "types/structs/WorkspaceFolder.hpp"
#include "types/structs/PublishDiagnosticsParams.hpp"
#include "types/structs/ClientCapabilities.hpp" 


#include "slang/ast/Compilation.h"
#include "slang/diagnostics/DiagnosticClient.h"
#include "slang/diagnostics/DiagnosticEngine.h"

#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <thread>



using json = nlohmann::json;




class DiplomatLSP : public slsp::BaseLSP
{
    protected:

        std::shared_ptr<DiplomatLSP> _this_shared;

        void _h_didChangeWorkspaceFolders(json params);
        void _h_didSaveTextDocument(json params);
        void _h_didOpenTextDocument(json params);
        json _h_gotoDefinition(json params);
        void _h_exit(json params);
        json _h_initialize(json params);
        void _h_initialized(json params);
        void _h_setTrace(json params);
        json _h_shutdown(json params);

        void _h_save_config(json params);
        void _h_set_top_module(json params);
        void _h_get_configuration(json& params);
        void _h_get_configuration_on_init(json& params);
        void _h_update_configuration(json& params);
        json _h_get_modules(json params);
        json _h_get_module_bbox(json params);
        void _h_ignore(json params);

        void _bind_methods();

        void _clear_diagnostics();
        void _cleanup_diagnostics();

        void _emit_diagnostics();
        void _erase_diagnostics();

        SVDocument* _read_document(std::filesystem::path path);

        std::unique_ptr<slang::SourceManager> _sm;

        std::unordered_map<std::filesystem::path, std::unique_ptr<SVDocument>> _documents;
        std::unordered_map<std::filesystem::path, std::string> _doc_path_to_client_uri;
        std::unordered_map<std::string, std::filesystem::path > _module_to_file;


        std::vector< std::filesystem::path> _root_dirs;


        std::unordered_set< std::filesystem::path> _excluded_paths;
        std::filesystem::path _settings_path;
        slsp::DiplomatLSPWorkspaceSettings _settings;

        std::shared_ptr<slsp::LSPDiagnosticClient> _diagnostic_client;

        std::unique_ptr<slsp::DiplomatIndex> _index;

        std::string _top_level;

        void _add_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_add);
        void _remove_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_rm);

        void _read_workspace_modules();
        void _compile();
        
        std::thread _pid_watcher;
        void _save_client_uri(const std::string& client_uri);

        std::unique_ptr<slang::ast::Compilation> _compilation;
        slsp::types::ClientCapabilities _client_capabilities;


    public:
        explicit DiplomatLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        slang::ast::Compilation* get_compilation();
        inline const std::unordered_map<std::filesystem::path, std::unique_ptr<SVDocument>>& get_documents() const {return _documents;};
        
        //void read_config(std::filesystem::path& filepath);
        void hello(json params);



};