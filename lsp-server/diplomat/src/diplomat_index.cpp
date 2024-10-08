

#include "diplomat_index.hpp"


#include "slang/ast/Scope.h"
#include "slang/ast/Compilation.h"
#include "slang/text/SourceLocation.h"
#include "fmt/format.h"
#include <functional>



namespace fs = std::filesystem;
namespace syntax = slang::syntax;
namespace ast = slang::ast;

using json = nlohmann::json;

namespace slsp
{
    bool SyntaxNodePtrCmp::operator()(const syntax::ConstTokenOrSyntax& lhs, const syntax::ConstTokenOrSyntax& rhs) const
    {
        const slang::SourceRange range_lhs = CONST_TOKNODE_SR(lhs);
        const slang::SourceRange range_rhs = CONST_TOKNODE_SR(rhs);
        return range_lhs.start() < range_rhs.start();
    }

     DiplomatIndex::DiplomatIndex(const slang::SourceManager* sm):
        _sm(sm),
        _index(),
        _definition_table(),
        _reference_table()
     {}


    /**
     * @brief Get the internal index value from the provided filepath
     * 
     * @param filepath File path to process
     * @return Index_FileID_t Index that can be used immediately.
     */
    Index_FileID_t DiplomatIndex::_index_from_filepath(const fs::path& filepath) const
    {
        return fs::canonical(filepath);
    }

    Index_FileID_t DiplomatIndex::_index_from_symbol(const slang::ast::Symbol &symbol) const
    {
        const slang::SourceManager* sm = symbol.getParentScope()->getCompilation().getSourceManager();
        return _index_from_filepath(sm->getFullPath(symbol.location.buffer()));
    }

    /**
     * @brief Ensure that a given index key exists in the index. 
     * Add it if it does not exists, do nothing otherwise.
     * 
     * @param idx Index key to lookup.
     */
    Index_FileID_t DiplomatIndex::_ensure_index(const Index_FileID_t &idx)
    {
        if(!_index.contains(idx))
        {
            _index[idx] = {};
        }

        return idx;
    }

    /**
     * @brief Ensure that the file exists in the index. 
     * Add the file if it does not exists, do nothing otherwise.
     * 
     * @param fileref Filepath to reference.
     * @return The index created or looked up.
     */
    Index_FileID_t DiplomatIndex::ensure_file(const fs::path& fileref)
    {
        return _ensure_index(_index_from_filepath(fileref));
    }

    /**
     * @brief Adds a symbol to the index
     * 
     * @param symbol Symbol to add
     * @param reffile File to add the symbol to
     */
    void DiplomatIndex::add_symbol(const slang::ast::Symbol& symbol)
    {
        const slang::syntax::ConstTokenOrSyntax anchor(symbol.getSyntax());
        if (anchor.isNode() && anchor.node() == nullptr)
            return;
        
        add_symbol(symbol,anchor);
    }


    void DiplomatIndex::add_symbol(const slang::ast::Symbol& symbol,  const slang::syntax::ConstTokenOrSyntax& anchor)
    {
        Index_FileID_t sid = _index_from_symbol(symbol);
        _ensure_index(sid);
            const unsigned int symbol_line = _sm->getLineNumber(CONST_TOKNODE_SR(anchor).start());
        if(! _index[sid].contains(symbol_line))
            _index.at(sid)[symbol_line] = {anchor};
        else
            _index.at(sid).at(symbol_line).insert(anchor);

        if(!_reference_table.contains(&symbol))
            _reference_table[&symbol] = {anchor};

        _definition_table[CONST_TOKNODE_SR(anchor)] = &symbol;
    }

    /**
     * @brief Add a reference to a symbol. This form deduces the file by using the token/syntax node.
     * 
     * @param symbol Symbol to refer to
     * @param ref Reference to record
     */
    void DiplomatIndex::add_reference_to(const slang::ast::Symbol& symbol, const syntax::ConstTokenOrSyntax& ref )
    {
        add_reference_to(symbol,ref,_sm->getFullPath(CONST_TOKNODE_SR(ref).start().buffer()));
    }


    /**
     * @brief Add a reference to a symbol.
     * 
     * @param symbol Symbol to refer to.
     * @param ref Reference to record.
     * @param reffile File where the reference is located.
     */
    void DiplomatIndex::add_reference_to(const slang::ast::Symbol& symbol, const syntax::ConstTokenOrSyntax& ref,const std::filesystem::path& reffile )
    {
        const slang::ast::Symbol* symbol_to_target = &symbol;
        if(! is_registered(symbol_to_target))
        {
            symbol_to_target = get_symbol_from_exact_range(symbol_to_target->getSyntax()->sourceRange());
            if(symbol_to_target == nullptr)
            {
                add_symbol(symbol);
                symbol_to_target = &symbol;
            }
            //_instance_type_symbol = _index->get_symbol_from_exact_range(CONST_TOKNODE_SR(*reference_node));
        }
            
        
        _reference_table[symbol_to_target].insert(ref);
        _definition_table[CONST_TOKNODE_SR(ref)] = symbol_to_target;

        Index_FileID_t rid = ensure_file(reffile);
        unsigned int ln = _sm->getLineNumber(CONST_TOKNODE_SR(ref).start());
        if(! _index.at(rid).contains(ln))
            _index.at(rid)[ln] = {};
        _index.at(rid).at(ln).insert(ref);
    }

    /**
     * @brief Check if a symbol has already been registered in the index
     * 
     * @param symbol Symbol to lookup 
     * @return true  if found
     */
    bool DiplomatIndex::is_registered(const slang::ast::Symbol& symbol) const
    {
        return _reference_table.contains(&symbol);
    }

    /**
     * @brief Check if a symbol has already been registered in the index
     * 
     * @param symbol Symbol to lookup 
     * @return true  if found
     */
    bool DiplomatIndex::is_registered(const slang::ast::Symbol* symbol) const
    {
        return _reference_table.contains(symbol);
    }


    /**
     * @brief Check if  syntax node has been registered in the index. 
     * 
     * @param node Syntax node to lookup 
     * @return true if found.
     */
    bool DiplomatIndex::is_registered(const syntax::ConstTokenOrSyntax& node) const
    {
        return _definition_table.contains(CONST_TOKNODE_SR(node));
    }

    /**
     * @brief Check if a file has been registered in the index
     * 
     * @param fileref File path to lookup
     * @return true if the file is registered
     */
    bool DiplomatIndex::is_registered(const fs::path& fileref) const
    {
        return _index.contains(_index_from_filepath(fileref));
    }


    /**
     * @brief Fetch a syntax node or token based upon a source range.
     * 
     * @param range Range to lookup
     * @return const syntax::ConstTokenOrSyntax* if an element is found which include the range (boundary included). 
     * @return nullopt otherwise.
     */
    std::optional<syntax::ConstTokenOrSyntax> DiplomatIndex::get_syntax_from_range(const slang::SourceRange& range) const
    {
        fs::path source_path = _sm->getFullPath(range.start().buffer());

        if(! is_registered(source_path))
            return std::nullopt;

        Index_FileID_t fid = _index_from_filepath(source_path);
        unsigned int line = _sm->getLineNumber(range.start());

        if (!_index.at(fid).contains(line))
            return std::nullopt;
        
        // Lookup matches only if looked up range is within the node.
        for(const syntax::ConstTokenOrSyntax& elt : _index.at(fid).at(line))
        {
            if(range.start() >= CONST_TOKNODE_SR(elt).start() && range.end() <= CONST_TOKNODE_SR(elt).end())
                return elt;
        }

        return std::nullopt;
    }

    std::optional<syntax::ConstTokenOrSyntax> DiplomatIndex::get_syntax_from_position(const std::filesystem::path& file, unsigned int line, unsigned int character) const
    {
        if(! is_registered(file))
            return std::nullopt;

        Index_FileID_t fid = _index_from_filepath(file);

        // Lookup matches only if looked up range is within the node.
        if (!_index.at(fid).contains(line))
            return std::nullopt;
        
        for (const syntax::ConstTokenOrSyntax& elt : _index.at(fid).at(line))
        {
            if (character >= _sm->getColumnNumber(CONST_TOKNODE_SR(elt).start()) && character <= _sm->getColumnNumber(CONST_TOKNODE_SR(elt).end()))
                return elt;
        }

        return std::nullopt;
    }
    
    /**
     * @brief Get a registered symbol pointer from its exact range.
     * 
     * This is used to query the actual definition table,
     * and might be used when registering references to 'virtual' symbols.
     * See module name registration method for an example of use.
     * 
     *
     * @param range range (key) to lookup.
     * @return the symbol or nullptr.
     */
    const slang::ast::Symbol* DiplomatIndex::get_symbol_from_exact_range(const slang::SourceRange& range) const
    {
        if(_definition_table.contains(range))
            return _definition_table.at(range);
        else
            return nullptr;
    }

    /**
     * @brief Get the source location of the definition of the syntax matching the provided range.
     * 
     * @param range Range to lookup
     * @return slang::SourceRange of the symbol definition if found.
     * @return slang::SourceRange::NoLocation otherwise.
     */
    slang::SourceRange DiplomatIndex::get_definition(const slang::SourceRange& range) const
    {
        auto matching_element = get_syntax_from_range(range);
        slang::SourceRange ret_value = slang::SourceRange::NoLocation;
        
        if(matching_element && is_registered(*matching_element))
            ret_value = _definition_table.at(matching_element->range())->getSyntax()->sourceRange();
        
        return ret_value;
    }

    /**
     * @brief Get the source location of the definition of the syntax matching the provided range.
     * 
     * @param range Range to lookup
     * @return slang::SourceRange of the symbol definition if found.
     * @return slang::SourceRange::NoLocation otherwise.
     */
    std::vector<slang::SourceRange> DiplomatIndex::get_references(const slang::SourceRange& range) const
    {
        auto matching_element = get_syntax_from_range(range);
        std::vector<slang::SourceRange> ret_value;

        if(matching_element  && is_registered(*matching_element))
        {
            const ast::Symbol* symb = _definition_table.at(matching_element->range());

            for(const syntax::ConstTokenOrSyntax& elt : _reference_table.at(symb))
                ret_value.push_back(elt.range());
        }

        return ret_value;        
    }

    std::vector<slang::syntax::ConstTokenOrSyntax > DiplomatIndex::get_symbols_tok_from_file(const std::filesystem::path& file) const
    {
        std::vector < slang::syntax::ConstTokenOrSyntax> ret;

        Index_FileID_t target_index = _index_from_filepath(file);
        Index_FileContent_t lu_file_content = _index.at(target_index);

        for (const auto& [line, lcontent] : lu_file_content)
        {
            for (const slang::syntax::ConstTokenOrSyntax& tok : lcontent)
                ret.push_back(tok);
        }

        return ret;

    }


    /**
     * @brief Delete all empty references in the index.
     * 
     */
    void DiplomatIndex::cleanup()
    {
        for(auto& [idx, fcontent] : _index)
        {
           std::erase_if(fcontent,[](const auto& item){
                auto const& [key, value] = item;
                return value.size() == 0;
           });
        }

        std::erase_if(_index,[](const auto& item){
            auto const& [key, value] = item;
            return value.size() == 0;
        });
    }

    json DiplomatIndex::dump()
    {
        json index;
        nlohmann::json_pointer<std::string> ptr;
        for (const auto& [file, fcontent] : _index)
        {
            ptr.push_back(std::string(file.generic_string()));

            for (const auto& [lineno, lcontent] : fcontent)
            {
                ptr.push_back(fmt::format("{}:{:d}",file.generic_string(),lineno));
                for (const slang::syntax::ConstTokenOrSyntax& elt : lcontent)
                {
                    std::string node_id;
                    if(elt.isNode())
                    {
                        node_id = "[S]-" + std::string(elt.node()->getFirstToken().rawText());
                    }
                    else
                    {
                        node_id = "[T]-" + std::string(elt.token().rawText());
                    }

                    ptr.push_back(node_id);

                    index[ptr] = fmt::format("{}:{}:{}:{}:{}",
                        file.generic_string(),
                        _sm->getLineNumber(elt.range().start()),
                        _sm->getColumnNumber(elt.range().start()),
                        _sm->getLineNumber(elt.range().end()),
                        _sm->getColumnNumber(elt.range().end())
                    );
                    ptr.pop_back();
                }
                ptr.pop_back();
            }
            ptr.pop_back();
        }

        return index;

    }


}