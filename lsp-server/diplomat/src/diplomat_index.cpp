

#include "diplomat_index.hpp"


#include "slang/ast/Scope.h"
#include "slang/ast/Compilation.h"
#include "slang/text/SourceLocation.h"

#include <functional>

namespace fs = std::filesystem;
namespace syntax = slang::syntax;
namespace ast = slang::ast;


/**
 * @brief Syntaxic sugar to get the source range of a ConstTokenOrSyntax
 * 
 * @param toknode The ConstTokenOrSyntax to handle. Does *not* support pointers.
 * Use `*toknode` in ordeer to handle them.
 */
#define CONST_TOKNODE_SR(toknode) ((toknode).isNode() ? (toknode).node()->sourceRange() : (toknode).token().range())


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
        return _index_from_filepath(sm->getFullPath(symbol.getSyntax()->getFirstToken().location().buffer()));
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
        Index_FileID_t sid = _index_from_symbol(symbol);

        _ensure_index(sid);

        const slang::syntax::ConstTokenOrSyntax syntax(symbol.getSyntax());
        const unsigned int symbol_line = _sm->getLineNumber(CONST_TOKNODE_SR(syntax).start());
        if(! _index[sid].contains(symbol_line))
            _index.at(sid)[symbol_line] = {syntax};
        else
            _index.at(sid).at(symbol_line).insert(syntax);

        if(!_reference_table.contains(&symbol))
            _reference_table[&symbol] = {syntax};

        _definition_table[CONST_TOKNODE_SR(syntax)] = &symbol;

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
        if(! is_registered(symbol))
            add_symbol(symbol);
        
        _reference_table[&symbol].insert(ref);
        _definition_table[CONST_TOKNODE_SR(ref)] = &symbol;

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

        // Lookup matches only if looked up range is within the node.
        for(const syntax::ConstTokenOrSyntax& elt : _index.at(fid).at(line))
        {
            if(range.start() >= CONST_TOKNODE_SR(elt).start() && range.end() <= CONST_TOKNODE_SR(elt).end())
                return elt;
        }

        return std::nullopt;
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
            ret_value = _definition_table.at(CONST_TOKNODE_SR(*matching_element))->getSyntax()->sourceRange();
        
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
            const ast::Symbol* symb = _definition_table.at(CONST_TOKNODE_SR(*matching_element));

            for(const syntax::ConstTokenOrSyntax& elt : _reference_table.at(symb))
                ret_value.push_back(CONST_TOKNODE_SR(elt));
        }

        return ret_value;        
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




}