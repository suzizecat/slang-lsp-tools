#include "fmt/format.h"
#include "slang/text/SourceManager.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include "diplomat_document_cache.hpp"

namespace fs = std::filesystem;
namespace diplomat::cache
{


/**
 * @note using `nullptr` instead of an actual pointer for \p bb will setup a binding but with an
 * empty list in the path_to_bb association.
 * This is made to maximise the integration and avoid specific existence checks downstream.
 */
void DiplomatDocumentCache::_bind_bb_and_path(const std::filesystem::path& fpath, const ModuleBlackBox* bb)
{
	if(! bb)
		_path_to_bb[fpath] = {};
	else
	{
		if(auto found = _path_to_bb.find(fpath); found != _path_to_bb.end())
		{
			found->second.push_back(bb);
		}
		else
		{
			_path_to_bb[fpath] = {bb};
		}

		_bb_to_path[bb] = fpath;
	}
}

void DiplomatDocumentCache::clear_project()
{
	_prj_files.clear();
	_prj_module_to_bb.clear();
}

void DiplomatDocumentCache::refresh(bool prj_only)
{
	for(const auto& fpath : _prj_files )
		process_file(fpath,true);

	if(! prj_only)
	{
		for (const auto& fpath : _ws_files)
			process_file(fpath,false);
	}
}

/**
 * @note This function does nothing on as long as the source_manager is enabled.
 */
void DiplomatDocumentCache::enable_shared_source_manager()
{
	if(_sm.get() == nullptr)
		_sm.reset(new slang::SourceManager());
}

void DiplomatDocumentCache::disable_shared_source_manager()
{
	_sm.reset();
}

void DiplomatDocumentCache::record_file(const fs::path& fpath,
										bool in_prj)
{
	fs::path canon_path = _standardize_path(fpath);
	if(in_prj)
	{
	   _prj_files.insert(canon_path);

	}
	
	// The file is always added to the WS files, but the lookup
	// will always start by the _prj_files.
	// However, if the file was already available as WS file and is read back into project,
	// The BB references should be pushed to the project.
	if( auto inserted = _ws_files.insert(canon_path); in_prj && ! inserted.second)
	{
		for(const ModuleBlackBox* bb : _path_to_bb.at(canon_path))
		{
			_prj_module_to_bb[bb->module_name] = bb;
		}
	}
	
}

const ModuleBlackBox* DiplomatDocumentCache::get_bb_by_module(const std::string& modname) const 
{
	if(auto lookup = _prj_module_to_bb.find(modname); lookup != _prj_module_to_bb.end())
	{
		return lookup->second;
	}
	else if(auto lookup = _ws_module_to_bb.find(modname); lookup != _ws_module_to_bb.end())
	{
		// Return any, should select based upon signature (that should be provided as param)
		return *(lookup->second.begin());
	}
	else 
	{
		return nullptr;
	}
}


const std::vector<const ModuleBlackBox*>* DiplomatDocumentCache::get_bb_by_file(const std::filesystem::path& fpath) const
{
	fs::path lu_path = _standardize_path(fpath);

	if(const auto found = _path_to_bb.find(lu_path); found != _path_to_bb.end() )
	{
		return &(found->second);
	}
	else 
	{
		return nullptr;
	}
}


/**
 * This function does not requires a canonical path and will not record the file if not recorded.
 */
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
	spdlog::debug("Request for cache processing {}",fpath.generic_string());
	fs::path curr_path = _standardize_path(fpath);


	// If the passed file has been already processed, check if the 
	// file has been modified before processing it.
	if(const auto found = _processed_timestamp.find(curr_path); found != _processed_timestamp.end())
	{
		if(fs::last_write_time(curr_path) <= found->second)
		{
			// Update the "in_prj" status and exit
			record_file(curr_path,in_prj);
			return;
		}
		else
			remove_file(curr_path);
	}

	// If the file has never been processed (or need recomputing)
	// We will need to parse it and map the appropriates infos at the right places.

	if(auto_dispose)
		_sm.reset(new slang::SourceManager());

	auto st = slang::syntax::SyntaxTree::fromFile(fpath.generic_string(),*_sm).value();
	VisitorModuleBlackBox visitor;
	st->root().visit(visitor);

	if(visitor.read_bb->empty())
	{
		// If no blackbox are present, there is a need to record the file in the path to bb association to
		// avoid issues downstream and maintain coherent behavior across all files.
		_bind_bb_and_path(curr_path,nullptr);
	}
	else 
	{
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

				_bind_bb_and_path(curr_path,bb_ptr);
			}        
		}

	}
   
	_processed_timestamp[curr_path] = std::chrono::file_clock::now();
	record_file(curr_path,in_prj);

	if(auto_dispose)
		_sm.reset();

}


/**
 * This function will add a mapping between the standardized path that
 * matches \p client_uri and \p client_uri itself.
 * This will not attemps to actually match the URI to a recorded file.
 * 
 * @note this function is assuming a properly constructed URI that will target an
 * accessible file.
 */
fs::path DiplomatDocumentCache::record_uri(const uri& client_uri)
{
	fs::path tgt_path = _standardize_path(fs::path(client_uri.get_path()));
	_doc_path_to_client_uri.insert_or_assign(tgt_path,client_uri);
	//_doc_path_to_client_uri[tgt_path] = uri(client_uri);

	return tgt_path;
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
			if(prj_bb != _prj_module_to_bb.cend() && prj_bb->second == bb)
			{  
				_prj_module_to_bb.erase(prj_bb);
			}
			

			_bb_to_path.erase(bb);

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
		_processed_timestamp.erase(path);
	}
}
} // namespace diplomat::cache