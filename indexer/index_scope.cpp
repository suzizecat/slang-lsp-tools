#include "index_scope.hpp"
#include <ranges>
#include "fmt/format.h"



// Custom specialization of std::hash can be injected in namespace std.
template<>
struct std::hash<diplomat::index::IndexScope>
{
    std::size_t operator()(const diplomat::index::IndexScope& s) const noexcept
    {
       return s.get_hash_value();
    }
};

namespace diplomat::index {


	IndexScope::IndexScope(std::string name, bool is_virtual, bool anonymous) : 
    _name(name),
    _is_virtual(is_virtual),
    _parent(nullptr),
	_anonymous(anonymous),
	_unnamed_count(0)
    {
    }

	IndexScope* IndexScope::add_child(const std::string& name, const bool isvirtual)
	{
		std::string used_name = name;
		if(name.empty())
			used_name = fmt::format("unnamed{}",_unnamed_count++);

		if(_children.contains(used_name))
			return _children.at(used_name).get();
		
		std::unique_ptr<IndexScope> up = std::make_unique<IndexScope>(used_name, isvirtual, name.empty());
		IndexScope* elt = up.get();
		up->_parent = this;
		_children[used_name] = std::move(up);

		// Once the hierarchical path is built, (re)compute the hash value.
		elt->compute_hash_value();

		return elt;
	}

	void IndexScope::add_symbol(IndexSymbol* symb)
	{
		_content[symb->get_name()] = symb;
	}

	IndexSymbol* IndexScope::lookup_symbol(const std::string &name, bool strict)
	{

		if( _content.contains(name))
		{
			return _content.at(name);
		}
		else if(! strict)
		{
			if(get_parent_access() && _parent != nullptr)
			{
				return _parent->lookup_symbol(name,false);
			}
		}
		
		return nullptr;
		
	}

	IndexSymbol* IndexScope::resolve_symbol(const std::string_view& path)
	{
		std::size_t dot_pos = path.find('.');
		// npos => not found
		if(std::string::npos == dot_pos)
			return lookup_symbol(std::string(path),true);
		else
		{
			std::string_view direct_lu = path.substr(0,dot_pos);
			IndexScope* next_scope;
			if((next_scope = get_scope_by_name(direct_lu)) != nullptr)
			{
				std::string_view remaining_path = path.substr(dot_pos+1);
				return next_scope->resolve_symbol(remaining_path);
			}
			else
				return nullptr;
		}

	}

	IndexScope *IndexScope::get_scope_for_location(const IndexLocation &loc, bool deep)
	{
		if(deep)
		{
			IndexScope* ret;
			for(auto& [key, value] : _children)
			{
				ret = value->get_scope_for_location(loc);
				if(ret != nullptr)
					return ret;
			}
			
			if( _source_range && _source_range.value().contains(loc))
				return this;
			else
				return nullptr;
		}
		else
		{
			if(! _source_range || ! _source_range.value().contains(loc))
				return nullptr;
			else
			{
				IndexScope* ret;
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

	IndexScope *IndexScope::get_scope_for_range(const IndexRange &loc, bool deep)
	{
		// Preliminary check, for fast exact maching test
		if(_source_range && loc == _source_range)
			return this;

		if(deep)
		{
			IndexScope* ret;
			for(auto& [key, value] : _children)
			{
				ret = value->get_scope_for_range(loc,true);
				if(ret != nullptr)
					return ret;
			}
		}
		else
		{
			for(auto& [key, value] : _children)
			{
				if(value->_source_range && value->_source_range.value().contains(loc))
					return value.get();
			}
		}

		// Finally, if the target range is not found in a child scope, check
		// if it is included in self.
		if( _source_range && _source_range.value().contains(loc))
				return this;
			else
				return nullptr;
	}

	IndexScope* IndexScope::get_child_by_exact_range(const IndexRange& loc)
	{
		for(auto& child : std::views::values(_children))
			if(child->get_source_range() && child->get_source_range().value() == loc)
				return child.get();

		return nullptr;
	}

	IndexScope* IndexScope::get_scope_by_name(const std::string_view& name)
	{
		std::string strname(name);
		if(_children.contains(strname))
			return _children.at(strname).get();
			
		return nullptr;
	}

	std::string IndexScope::get_full_path() const
	{
		if(_parent != nullptr)
			return _parent->get_full_path() + "." + _name;
		else
			return _name;
	}

	std::string IndexScope::get_concrete_path() const 
	{
		if (_is_virtual)
		{
			 if(_parent != nullptr)
				return _parent->get_concrete_path();
			else
				return "";
		}
		else
		{
			if(_parent != nullptr)
				return _parent->get_concrete_path() + "." + _name;
			else
				return _name;
		}
			
	}


	void IndexScope::_build_concrete_children(std::set<IndexScope*>& ret_holder, bool is_root)
	{
		// If we are running the first iteration, we don't want to return ourself.
		if(! is_root && ! _is_virtual )
			ret_holder.insert(this);
		else
		{
			for(auto& child : std::views::values(_children))
				child->_build_concrete_children(ret_holder,false);
		}
	}


	std::set<IndexScope*> IndexScope::get_concrete_children()
	{
		std::set<IndexScope*> ret;
		_build_concrete_children(ret);
		return ret;
	}

	size_t IndexScope::compute_hash_value()
	{
		_hash_value = std::hash<std::string>{}(get_full_path());
		return _hash_value;
	}


	void to_json(nlohmann::json &j, const IndexScope &s)
{

	j = nlohmann::json{
		#ifdef DIPLOMAT_DEBUG
		{"_kind",s._kind},
		#endif
		{"name",s._name},
		{"def",s._source_range},
		{"virtual",s._is_virtual},
		{"children",s._children}
	};


	nlohmann::json content;

	for(auto& [key, value] : s._content)
	{
		content[key] = *value;
	}
	j["content"] = content ;
}

// void from_json(const nlohmann::json &j, IndexScope &s)
// {
// 		JSON_TO_STRUCT_SAFE_BIND(j,"name",s._name);
// 		JSON_TO_STRUCT_SAFE_BIND(j,"def",s._source_range);
// 		// JSON_TO_STRUCT_SAFE_BIND(j,"content",s._content);
// 		// JSON_TO_STRUCT_SAFE_BIND(j,"sub",s._children);
// 		JSON_TO_STRUCT_SAFE_BIND(j,"virtual",s._is_virtual);
// 		JSON_TO_STRUCT_SAFE_BIND(j,"parentAccess",s._can_access_parent);

// 		for (auto& [key, value] : j["content"].items())
// 		{
// 			IndexSymbol* newSymb = s.add_symbol(key);
// 			*newSymb = value.template get<IndexSymbol>();
// 		}

// 		// Might require some more subtle approach...
// 		for (auto& [key, value] : j["sub"].items())
// 		{
// 			IndexScope* new_scope = s.add_child(key);
// 			*new_scope = value.template get<IndexScope>();
// 		}
// }
};
