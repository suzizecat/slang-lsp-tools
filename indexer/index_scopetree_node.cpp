#include "index_scopetree_node.hpp"
#include "fmt/format.h"
#include "index_exceptions.hpp"
#include "index_scope.hpp"
#include "index_symbols.hpp"
#include <memory>
#include <string_view>
#include <ranges>

namespace diplomat::index {

	IndexScopeTreeNode::IndexScopeTreeNode(std::string name, IndexScope* data, IndexScopeTreeNode* parent, bool isvirtual) : 
		_name(name),
		_data(data),
		_children(),
		_parent(nullptr),
		_is_virtual(isvirtual),
		_unnamed_count(0)
		{
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

	IndexScopeTreeNode* IndexScopeTreeNode::add_child(const std::string& name, IndexScope* data, const bool is_virtual)
	{
		// Ensure that the unique_ptr is destroyed in the process if something goes wrong
		std::unique_ptr<IndexScopeTreeNode> new_child = std::make_unique<IndexScopeTreeNode>(std::string(name), data, this, is_virtual);
		return _attach_child(std::move(new_child));
	}	

	IndexScopeTreeNode* IndexScopeTreeNode::add_anon_child(IndexScope* data, const bool is_virtual)
	{
		// Ensure that the unique_ptr is destroyed in the process if something goes wrong
		std::unique_ptr<IndexScopeTreeNode> new_child = std::make_unique<IndexScopeTreeNode>(_get_unnamed_id(), data, this, is_virtual);
		return _attach_child(std::move(new_child));
	}	

	void IndexScopeTreeNode::add_symbol(IndexSymbol* symbol)
	{
		_data->add_symbol(symbol);
	}


	IndexSymbol* IndexScopeTreeNode::lookup_symbol(const std::string_view &name, bool strict)
	{

		IndexSymbol* lu_result = _data->lookup_symbol(std::string{name});
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
			return _data->lookup_symbol(std::string(path),true);
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

	IndexScopeTreeNode* IndexScopeTreeNode::get_scope_by_name(const std::string_view& name)
	{
		auto result = _children.find(std::string(name));
		if(result != _children.end())
			return result->second.get();
		else
			return nullptr;
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
			if(_data->is_virtual()) {
				return "";
			} else {
				return _name;
			}
		}
		else{
			if(_data->is_virtual())
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
}