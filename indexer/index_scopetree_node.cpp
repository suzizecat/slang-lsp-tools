#include "index_scopetree_node.hpp"
#include "fmt/format.h"
#include "index_exceptions.hpp"
#include "index_scope.hpp"
#include "index_symbols.hpp"
#include "nlohmann/json_fwd.hpp"
#include <memory>
#include <string_view>
#include <ranges>

namespace diplomat::index {

	IndexScopeTreeNode::IndexScopeTreeNode(
		std::shared_ptr<IndexScope> data, 
		std::optional<std::string> name,  
		IndexScopeTreeNode* parent,  
		bool isvirtual	) :
		_data(data ? data : std::make_shared<IndexScope>()),
		_children(),
		_parent(nullptr),
		_is_virtual(isvirtual),
		_unnamed_count(0),
		_valid(false)
		{
			if(!name)
			{
				if(parent)
					_name = parent->_get_unnamed_id();
				else
					_name = "MISSING_NAME";
			}
			else
			{
				_name = name.value();
			}

			if(parent)
				parent->_attach_child(this);

		}


	std::string IndexScopeTreeNode::_get_unnamed_id()
	{
		return fmt::format("unnamed{}", _unnamed_count ++);
	}

	void IndexScopeTreeNode::_attach_child(IndexScopeTreeNode* new_child)
	{
		if(new_child->_parent && new_child->_parent != this)
			throw not_orphan_node_error(fmt::format("Trying to attach non-orphan node {} to {}", new_child->get_full_path(), this->get_full_path()));

		auto [new_elt, ok] = _children.try_emplace(new_child->_name, std::unique_ptr<IndexScopeTreeNode>(new_child));
		if(! ok)
			throw index_exception(fmt::format("Unable to add new children {} into {}", new_child->get_name(), get_full_path()));

		new_child->_parent = this;
	}

	IndexScopeTreeNode* IndexScopeTreeNode::_attach_child(std::unique_ptr<IndexScopeTreeNode> new_child)
	{
		if(new_child->_parent && new_child->_parent != this)
			throw not_orphan_node_error(fmt::format("Trying to attach non-orphan node {} to {}", new_child->get_full_path(), this->get_full_path()));

		if(_children.contains(new_child->get_name()))
			throw index_exception(fmt::format("Unable to add new children {} into {}", new_child->get_name(), get_full_path()));

		new_child->_parent = this;
		auto [it, _] = _children.emplace(new_child->get_name(),std::move(new_child));
		return it->second.get();
	}

	IndexScopeTreeNode* IndexScopeTreeNode::add_child(const std::string& name, std::shared_ptr<IndexScope> data, const bool is_virtual)
	{
		auto lu_result = _children.find(name); 
		if(lu_result != _children.end())
			return lu_result->second.get();
		else 
		{
			std::unique_ptr<IndexScopeTreeNode> new_child = std::make_unique<IndexScopeTreeNode>(data, name, nullptr, is_virtual);
			return _attach_child(std::move(new_child));
		}
	}	

	
	IndexScopeTreeNode* IndexScopeTreeNode::add_anon_child(std::shared_ptr<IndexScope>  data, const bool is_virtual)
	{
		std::unique_ptr<IndexScopeTreeNode> new_child = std::make_unique<IndexScopeTreeNode>(data, _get_unnamed_id(), nullptr, is_virtual);
		return _attach_child(std::move(new_child));
	}	


	IndexScopeTreeNode* IndexScopeTreeNode::add_subtree_child(const IndexScopeTreeNode* reference, const std::string& name)
	{
		IndexScopeTreeNode* fl_child = add_child(name, reference->_data, reference->_is_virtual);
		
		// Should be useless, but will avoid incoherency and thus downstream dumb issues 
		fl_child->_unnamed_count = reference->_unnamed_count;

		for(const auto& [cname, cref] : reference->_children)
		{
			fl_child->add_subtree_child(cref.get() ,cname);
		}
		
		if(reference->is_valid())
			fl_child->validate();

		return fl_child;
	}	


	IndexSymbol* IndexScopeTreeNode::add_symbol(IndexSymbol* symbol)
	{
		return _data->add_symbol(symbol);
	}

	IndexSymbol* IndexScopeTreeNode::add_symbol(std::unique_ptr<IndexSymbol> symbol)
	{
		return _data->add_symbol(std::move(symbol));
	}

	IndexSymbol* IndexScopeTreeNode::lookup_symbol(const std::string_view &name, bool strict)
	{

		IndexSymbol* lu_result = _data->get_symbol(std::string{name});
		if(lu_result)
		{
			return lu_result;
		}
		else if(! strict)
		{
			if(have_parent_access() && _parent != nullptr)
			{
				return _parent->lookup_symbol(name,false);
			}
		}
		
		return nullptr;
	}

	IndexSymbol* IndexScopeTreeNode::resolve_symbol(const std::string_view& path)
	{
		std::size_t dot_pos = path.rfind('.');
		// npos => not found
		if(dot_pos == std::string::npos)
			return _data->get_symbol(std::string(path));
		else
		{

			std::string_view direct_lu = path.substr(0,dot_pos);
			IndexScopeTreeNode* next_scope;
			if((next_scope = resolve_scope(direct_lu)) != nullptr)
			{
				std::string_view remaining_path = path.substr(dot_pos+1);
				return next_scope->lookup_symbol(remaining_path);
			}
			else
				return nullptr;
		}

	}

	IndexScopeTreeNode* IndexScopeTreeNode::resolve_scope(const std::string_view& path)
	{
		std::size_t dot_pos = path.find('.');
		if(dot_pos == std::string::npos)
			return get_scope_by_name(std::string(path));
		else
		{
			std::string_view direct_lu = path.substr(0,dot_pos);
			IndexScopeTreeNode* next_scope;
			if((next_scope = get_scope_by_name(direct_lu)) != nullptr)
			{
				std::string_view remaining_path = path.substr(dot_pos+1);
				return next_scope->resolve_scope(remaining_path);
			}
			else
				return nullptr;
		}
		return nullptr;
	}

	std::vector<const IndexSymbol*> IndexScopeTreeNode::get_visible_symbols(std::optional<IndexLocation> exact) const
	{
		const IndexScopeTreeNode* lu_scope = this;
		std::vector<const IndexSymbol*> ret;
		bool keep_going = false;
		do {
			
			for(auto& symb : lu_scope->_data->get_symbols())
			{
				// If exact is not provided (no filter), symbol location does not exist (dunno why)
				// or the symbol has been defined before 'exact', then it is added to the output list.
				if(! exact || !symb->get_source_location() || exact.value() >= symb->get_source_location().value() )
					ret.push_back(symb.get());
			}

			if(lu_scope->have_parent_access())
			{		
				lu_scope = lu_scope->_parent;
				keep_going = true;
			}
			else
			{
				keep_going = false;
			}
		
		} while (keep_going);

		return ret;
	}

	IndexScopeTreeNode* IndexScopeTreeNode::get_scope_for_location(const IndexLocation &loc, bool deep)
	{
		if(deep)
		{
			// When in 'deep' mode, we may be looking up a ascope that would be in another file
			// altogether
			// therefore, we try to go as deep as needed in the scope tree.
			// This will be *very* expensive on big trees.
			IndexScopeTreeNode* ret;
			
			if( _data->get_source_range() && _data->get_source_range().value().contains(loc))
			{
				// If the current scope matches somehow the location, stop descending deep
				// as we only want the higher match in the tree (no use for the lowest match)
				return get_scope_for_location(loc, false);
			}
			else
			{
				// If we are descending deep, but the current scope does not match, descend in all
				// children.
				for(auto& [key, value] : _children)
				{
					ret = value->get_scope_for_location(loc, true);
					if(ret != nullptr)
						return ret;
				}

				return nullptr;
			}

		}
		else
		{
			if(!  _data->get_source_range() || !  _data->get_source_range().value().contains(loc))
				return nullptr;
			else
			{
				IndexScopeTreeNode* ret;
				for(auto& [key, value] : _children)
				{
					ret = value->get_scope_for_location(loc);
					if(ret != nullptr)
						return ret;
				}
					
				return this;
			}
		}
	}


	IndexScopeTreeNode *IndexScopeTreeNode::get_scope_for_range(const IndexRange &loc, bool deep)
	{
		// Preliminary check, for fast exact maching test
		if(_data->get_source_range() && loc == _data->get_source_range() )
			return this;

		if(deep)
		{
			// When in 'deep' mode, we may be looking up a a scope that would be in another file
			// altogether
			// therefore, we try to go as deep as needed in the scope tree.
			// This will be *very* expensive on big trees.
			IndexScopeTreeNode* ret;
			
			if( _data->get_source_range() && _data->get_source_range().value().contains(loc))
			{
				// If the current scope matches somehow the location, stop descending deep
				// as we only want the higher match in the tree (no use for the lowest match)
				return get_scope_for_range(loc, false);
			}
			else
			{
				// If we are descending deep, but the current scope does not match, descend in all
				// children.
				for(auto& [key, value] : _children)
				{
					ret = value->get_scope_for_range(loc, true);
					if(ret != nullptr)
						return ret;
				}

				return nullptr;
			}

		}
		else
		{
			if(!  _data->get_source_range() || !  _data->get_source_range().value().contains(loc))
				return nullptr;
			else
			{
				IndexScopeTreeNode* ret;
				for(auto& [key, value] : _children)
				{
					ret = value->get_scope_for_range(loc,false);
					if(ret != nullptr)
						return ret;
				}
					
				return this;
			}
		}
	}


	IndexScopeTreeNode* IndexScopeTreeNode::get_child_by_exact_range(const IndexRange& loc)
	{
		for(auto& child : std::views::values(_children))
		{
			if(child->get_source_range() == loc)
				return child.get();
		}

		return nullptr;
	}

	IndexScopeTreeNode* IndexScopeTreeNode::get_scope_by_name(const std::string_view& name, bool strict)
	{
		auto result = _children.find(std::string(name));
		if(result != _children.end())
			return result->second.get();
		else
			if(strict || ! have_parent_access()) 
				return nullptr;
			else 
				return _parent->get_scope_by_name(name, strict);
	}


	std::string IndexScopeTreeNode::get_full_path() const
	{
		if(! _parent)
		{
			return _name;
		}
		else
		{
			return  fmt::format("{}.{}", _parent->get_full_path(), _name);
		}
	}


	std::string IndexScopeTreeNode::get_concrete_path() const
	{
		if(! _parent)
		{
			if(is_virtual()) {
				return "";
			} else {
				return _name;
			}
		}
		else{
			if(is_virtual())
				return _parent->get_concrete_path();
			else 
			{
				std::string ret = _parent->get_concrete_path();
				
				if(ret.empty())
					return _name;
				else
					return  fmt::format("{}.{}", ret, _name);
			}
		}
	}


	const IndexScopeTreeNode* IndexScopeTreeNode::get_root() const
	{
		return _parent ? _parent->get_root() : this;
	}


	IndexScopeTreeNode* IndexScopeTreeNode::get_root()
	{
		return _parent ? _parent->get_root() : this;
	}


	void IndexScopeTreeNode:: cleanup()
	{
		for (auto iter = _children.begin(); iter != _children.end();) 
		{
			if(iter->second->is_valid())
			{
				iter->second->cleanup();
				iter ++;
			}
			else
			{
				iter = _children.erase(iter);
			}
		}
	}

	void to_json(nlohmann::json& j, const IndexScopeTreeNode& s)
	{
		j = nlohmann::json(); 
		// j["name"] = s._name;
		for(const auto & [ name, child ] : s._children)
			j[name] = child;

		j["z@data"] = *(s._data);
}
}
