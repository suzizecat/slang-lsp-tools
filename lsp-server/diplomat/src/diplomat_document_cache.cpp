#include "fmt/format.h"
#include "diplomat_document_cache.hpp"

namespace fs = std::filesystem;
namespace diplomat::cache
{
void DiplomatDocumentCache::record_file(const fs::path& fpath,
                                        bool in_prj)
{
    fs::path canon_path = _standardize_path(fpath);
    if(in_prj)
        _prj_files.emplace(fpath);
    
    // The file is always added to the WS files, but the lookup
    // will always start by the _prj_files.
    _ws_files.emplace(fpath);
}

uri DiplomatDocumentCache::get_uri(const std::filesystem::path& fpath) const
{
    fs::path stdpath = _standardize_path(fpath);

    // Example from https://en.cppreference.com/w/cpp/container/map/find.html
    if( auto result = _doc_path_to_client_uri.find(stdpath); result != _doc_path_to_client_uri.end())
        return result->second;
    else
        return uri(fmt::format("file://{}",stdpath.generic_string()));
}

void DiplomatDocumentCache::process_file(const std::filesystem::path& fpath, bool in_prj)
{
    bool auto_dispose = ! _sm.get();

    fs::path curr_path = _standardize_path(fpath);

    record_file(curr_path,in_prj);

    if(auto_dispose)
        _sm.reset(new slang::SourceManager());

    auto st = slang::syntax::SyntaxTree::fromFile(fpath.generic_string(),*_sm).value();
    VisitorModuleBlackBox visitor;
    st->root().visit(visitor);


    for(const auto modname : std::views::keys(*(visitor.read_bb.get())))
    {
        
        const auto insert_ok = _bb_storage.emplace((std::intptr_t)(visitor.read_bb->at(modname).get()),std::move(visitor.read_bb->at(modname)));
        const ModuleBlackBox* bb_ptr = insert_ok.first->second.get();
        
        if(_prj_files.contains(curr_path))
            _prj_module_to_bb[modname] = bb_ptr;
        else 
        {
            if(auto result = _ws_module_to_bb.find(modname) ; result != _ws_module_to_bb.end())
            {
                result->second.emplace(bb_ptr);
            }
            else
            {
                _ws_module_to_bb[modname] = {bb_ptr};
            }
        }        
    }

    if(auto_dispose)
        _sm.reset();

}


void DiplomatDocumentCache::remove_file(const std::filesystem::path& fpath)
{
    auto path = _standardize_path(fpath);

    if(_ws_files.contains(path))
    {
        for(const auto* bb : _path_to_bb.at(path) )
        {
            // Delete all blackbox references.
            const std::string& modname = bb->module_name;

            const auto prj_bb =  _prj_module_to_bb.find(modname);
            if(prj_bb != _prj_module_to_bb.cend())
            {
                _prj_module_to_bb.erase(prj_bb);
            }

            _ws_module_to_bb.at(modname).erase(bb);
            if(_ws_module_to_bb.at(modname).empty())
                _ws_module_to_bb.erase(modname);

            _bb_storage.erase((std::intptr_t)bb);

        }

        // Now that all blackboxes have been removed, delete the 
        // file references.
        _path_to_bb.erase(path);
        _doc_path_to_client_uri.erase(path);
        _prj_files.erase(path);
        _ws_files.erase(path);
    }
}
} // namespace diplomat::cache