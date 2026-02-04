#include "index_file.hpp"
#include "index_elements.hpp"
#include "index_scopetree_node.hpp"
#include "index_symbols.hpp"
#include <cstddef>
#include <memory>
#include <ranges>
#include <spdlog/spdlog.h>
#include <cassert>
namespace diplomat::index {
	IndexFile::IndexFile(const std::filesystem::path& path) :
	_filepath(std::filesystem::weakly_canonical(path)),
	_syntax_root{},
	_scopes{},
	_declarations(),
	_references(),
	_scopes_locations()
	{
		spdlog::debug("Call File constructor of {}", _filepath.generic_string());
	}


	IndexFile::~IndexFile()
	{
		spdlog::debug("Call File destructor of {}", _filepath.generic_string());
	}
	IndexSymbol* IndexFile::add_symbol(IndexSymbol* symb)
	{
		if(! symb->get_source()) 
		{
			spdlog::error("Tried to record symbol {} without source location to file {}", symb->get_name(),_filepath.generic_string());
			return nullptr;
		}
		else if(symb->get_source_location()->file != _filepath)
		{
			spdlog::error("Tried to record symbol {} to file {} with mismatching file path {}", symb->get_name(),_filepath.generic_string(), symb->get_source_location()->file.generic_string());
			return nullptr;
		}

		auto [eltpair, inserted] = _declarations.try_emplace(symb->get_source_location().value(),symb);


		// If the symbol was invalid, the reference needs to be pushed again.
		// This cover both the case where the symbol did not exists (invalid by default) and an invalidated symbol.
		if(! eltpair->second->is_valid())
		{
			// #ifdef DIPLOMAT_DEBUG
			// eltpair->second->set_kind(kind);
			// #endif
			add_reference(symb, symb->get_source().value(),true);
		}
		
		eltpair->second->validate();
		return eltpair->second;
	}
	

	void IndexFile::register_scope(IndexScopeTreeNode *_scope)
	{
		// Register the scope by name
		auto [inserted_scope, _] = _scopes.insert({_scope->get_full_path(),_scope});
		// It is mandatory to rebind scopes even if the insertion in _scopes failed.
		// That occurs in the case where we are incrementally rebuilding the scopetree.
		// In this case, the scopes are not removed from the file (but their locations are deleted).

		// Register the scope by range (if any)
		if(inserted_scope->second->get_source_range().has_value())
		{
			// The objective is to check if we are within a parent scope
			// If so, insert both:
			//  - The new scope at its designated location
			//  - A reference to the parent, after the end of the new scope for later lookup
			IndexRange scope_range = inserted_scope->second->get_source_range().value();
			auto parent_scope_key = _scopes_locations.upper_bound(scope_range.start);

			// If we indeed have a parent scope registered, insert the restart of the parent and then
			// the new scope.
			if(parent_scope_key != _scopes_locations.begin())
			{
				parent_scope_key --;
				
				if(parent_scope_key->second->get_source_range()->contains(scope_range))
				{
					// If we inserted within a parent scope, we add the parent scope at the end
					// of the newly inserted scope, to retain a proper lookup.
					IndexLocation new_end = scope_range.end;
					new_end.column ++;
					_scopes_locations.emplace_hint(parent_scope_key, new_end , parent_scope_key->second);
					
				}
				
			}	
			
			// Avoid updating the map before using the iterator for lookup.
			_scopes_locations.emplace_hint(parent_scope_key,scope_range.start, inserted_scope->second);
			
		}
	}

	IndexScopeTreeNode* IndexFile::lookup_scope_by_range(const IndexRange& range)
	{
		auto lu_result = _scopes_locations.upper_bound(range.start);
		
		while(lu_result != _scopes_locations.begin())
		{
			lu_result --;
			const auto& lu_range = lu_result->second->get_source_range();
			// If both boundaries are out of scope, exit the loop
			if(lu_range->contains(range))
				return lu_result->second;
			else if (! (lu_range->end < range.start) )
				return nullptr;
		}
		return nullptr;
		
	}

	IndexScopeTreeNode* IndexFile::lookup_scope_by_exact_range(const IndexRange& loc)
	{
		IndexScopeTreeNode* lu_result = lookup_scope_by_location(loc.start);
		if(lu_result && lu_result->get_source_range() == loc)
			return lu_result;
		return nullptr;
	}

	IndexScopeTreeNode* IndexFile::lookup_scope_by_location(const IndexLocation& loc)
	{
		auto lu_result = _scopes_locations.upper_bound(loc);

		// If upper_bound is not begin, the looked_up scope is at iterator minus one.
		// Lookup method from https://stackoverflow.com/a/45426884
		if(lu_result != _scopes_locations.begin())
			lu_result --;
		else 
			return nullptr;

		if(lu_result->second->get_source_range()->contains(loc))
			return lu_result->second;
		else 
			return nullptr;
	}

	IndexSymbol* IndexFile::lookup_symbol_by_location(const IndexLocation& loc)
	{
		// Lookup method from https://stackoverflow.com/a/45426884
		IndexScopeTreeNode* scope = lookup_scope_by_location(loc);
		if(! scope)
			return nullptr;
		
		auto lu_result = _references.upper_bound(loc);
		if(lu_result != _references.begin())
			lu_result--;
		else
			return nullptr;

		if(lu_result->second.loc.contains(loc))
		{
			return lu_result->second.key;
		}
		else
			return nullptr;
	}

	void IndexFile::add_reference(IndexSymbol* symb, const IndexRange& range, bool is_definition)
	{
		assert(range.start.file == _filepath);
		if(! _references.try_emplace(range.start,range,symb,is_definition).second)
		{
			spdlog::debug("    Duplicate reference to {}", symb->get_name());
		}
		else
		{
			if(! is_definition)
				symb->add_reference(range);
		}
	}

	void IndexFile::invalidate_file()
	{
		spdlog::info("Invalidating file {}", _filepath.generic_string());
		_valid = false;
		_syntax_root.reset();

		spdlog::debug("Clearing symbols and references");
		for(IndexSymbol* sym : _declarations | std::views::values )
		{
			sym->invalidate();
			// As the references are removed anyway just after, no need to iterate twice on the symbols.
			// sym->clear_local_references();
		}

		// Delete all reference in the current file in their respective symbols.
		// Once done, there should not be any active reference to the current file.
		for(ReferenceRecord& ref : _references | std::views::values )
		{
			ref.key->remove_reference(ref.loc);
		}
		_references.clear();

		// Need to clear the scopes
		spdlog::debug("Invalidating scope trees");

		for(IndexScopeTreeNode* scope : _scopes_locations | std::views::values )
		{
			scope->invalidate();
		}

		// Scopes locations will need to be bound again
		_scopes_locations.clear();

	}

	void IndexFile::remove_reference_by_location(const IndexLocation& loc)
	{
		_references.erase(loc);
	}

	void to_json(nlohmann::json &j, const IndexFile &s)
	{
		j = nlohmann::json {
			{"path", s._filepath}

		};

		j["scopes"] = nlohmann::json::array();
		j["symbols"] = nlohmann::json::array();

		for (const auto& [key, value] : s._scopes)
		{
			j["scopes"].push_back(key);
		}

		for (const auto& [key, value] : s._declarations)
		{
			j["symbols"].push_back(*value);
		}

		

		#ifdef DIPLOMAT_DEBUG
		if(s._failed_references.size() > 0)
		{
		j["reffailed"] = s._failed_references;
		
		spdlog::info("Dumped {} failed refs for file {}",s._failed_references.size(),s.get_path().generic_string());
		}
		#endif

	}
}
