#pragma once

#include "lsp.hpp"
#include "lsp_errors.hpp"
#include "sv_document.hpp"
#include "diagnostic_client.hpp"

#include "nlohmann/json.hpp"

#include "types/structs/ServerCapabilities.hpp"
#include "types/structs/WorkspaceFolder.hpp"
#include "types/structs/PublishDiagnosticsParams.hpp"

#include "slang/ast/Compilation.h"
#include "slang/diagnostics/DiagnosticClient.h"
#include "slang/diagnostics/DiagnosticEngine.h"

#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <filesystem>



using json = nlohmann::json;




class DiplomatLSP : public slsp::BaseLSP
{
    protected:

        std::shared_ptr<DiplomatLSP> _this_shared;

        void _h_didChangeWorkspaceFolders(json params);
        void _h_didSaveTextDocument(json params);
        void _h_exit(json params);
        json _h_initialize(json params);
        void _h_initialized(json params);
        void _h_setTrace(json params);
        json _h_shutdown(json params);

        void _h_set_top_module(json params);
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

        std::unordered_map<std::string, std::unique_ptr<SVDocument>> _documents;
        std::unordered_map<std::string, std::string > _module_to_file;

        std::vector< std::filesystem::path> _root_dirs;
        std::unordered_set< std::filesystem::path> _excluded_paths;

        std::shared_ptr<slsp::LSPDiagnosticClient> _diagnostic_client;

        std::string _top_level;

        void _add_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_add);
        void _remove_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_rm);

        void _read_workspace_modules();
        void _compile();
        
        

        std::unique_ptr<slang::ast::Compilation> _compilation;


    public:
        explicit DiplomatLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        slang::ast::Compilation* get_compilation();
        inline const std::unordered_map<std::string, std::unique_ptr<SVDocument>>& get_documents() const {return _documents;};
        

        void hello(json params);



};