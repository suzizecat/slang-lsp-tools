#pragma once

#include "lsp.hpp"
#include "lsp_errors.hpp"
#include "sv_document.hpp"
#include "nlohmann/json.hpp"

#include "types/structs/ServerCapabilities.hpp"
#include "types/structs/WorkspaceFolder.hpp"
#include "types/structs/PublishDiagnosticsParams.hpp"

#include "slang/ast/Compilation.h"
#include "slang/diagnostics/DiagnosticClient.h"
#include "slang/diagnostics/DiagnosticEngine.h"

#include <iostream>
#include <unordered_map>
#include <memory>
#include <filesystem>



using json = nlohmann::json;




class DiplomatLSP : public slsp::BaseLSP, public slang::DiagnosticClient
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

        void _bind_methods();

        SVDocument* _read_document(std::filesystem::path path);

        std::unique_ptr<slang::SourceManager> _sm;

        std::unordered_map<std::string, std::unique_ptr<SVDocument>> _documents;
        std::unordered_map<std::string, std::string > _module_to_file;

        std::vector< std::filesystem::path> _root_dirs;
        std::unordered_map<std::string, slsp::types::PublishDiagnosticsParams*> _diagnostics;

        std::string _top_level;

        void _add_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_add);
        void _remove_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_rm);

        void _read_workspace_modules();
        void _compile();
        
        

        std::unique_ptr<slang::ast::Compilation> _compilation;


    public:
        explicit DiplomatLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        slang::ast::Compilation* get_compilation();
        virtual void report(const slang::ReportedDiagnostic& diagnostic);

        void hello(json params);



};