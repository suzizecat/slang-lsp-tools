#pragma once

#include "lsp.hpp"
#include "lsp_errors.hpp"
#include "sv_document.hpp"
#include "nlohmann/json.hpp"

#include "types/structs/ServerCapabilities.hpp"
#include "types/structs/WorkspaceFolder.hpp"

#include <iostream>
#include <unordered_map>
#include <memory>
#include <filesystem>



using json = nlohmann::json;




class DiplomatLSP : public slsp::BaseLSP
{
    protected:
        void _h_didChangeWorkspaceFolders(json params);
        void _h_exit(json params);
        json _h_initialize(json params);
        void _h_initialized(json params);
        void _h_setTrace(json params);
        json _h_shutdown(json params);

        void _h_set_top_module(json params);
        json _h_get_modules(json params);
        json _h_get_module_bbox(json params);

        void _bind_methods();

        SVDocument* _read_document(std::string path);

        std::unordered_map<std::string, std::unique_ptr<SVDocument>> _documents;
        std::unordered_map<std::string, std::string > _module_to_file;

        std::vector< std::filesystem::path> _root_dirs;

        std::string _top_level;

        void _add_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_add);
        void _remove_workspace_folders(const std::vector<slsp::types::WorkspaceFolder>& to_rm);

        void _read_workspace_modules();

    public:
        explicit DiplomatLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        void hello(json params);


};