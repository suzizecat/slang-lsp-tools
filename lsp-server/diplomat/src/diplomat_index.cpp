#include "diplomat_index.hpp"


#include "slang/ast/Scope.h"
#include "slang/ast/Compilation.h"
#include "slang/text/SourceLocation.h"
#include "slang/text/SourceManager.h"

namespace fs = std::filesystem;

namespace slsp
{
    bool SyntaxNodePtrCmp::operator()(const slang::syntax::SyntaxNode *lhs, const slang::syntax::SyntaxNode *rhs) const
    {
        return lhs->sourceRange().start() < rhs->sourceRange().start();
    }

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
        const slang::SourceManager* sm = symbol.getParentScope()->getCompilation().getSourceManager();

        Index_FileID_t sid = _index_from_symbol(symbol);

        _ensure_index(sid);

        const slang::syntax::SyntaxNode* syntax = symbol.getSyntax();
        const unsigned int symbol_line = sm->getLineNumber(syntax->sourceRange().start());
        if(! _index[sid].contains(symbol_line))
            _index.at(sid)[symbol_line] = {syntax};
        else
            _index.at(sid).at(symbol_line).insert(syntax);

        if(!_reference_table.contains(&symbol))
            _reference_table[&symbol] = {};
    }

    /**
     * @brief Add a reference to a symbol.
     * 
     * @param symbol Symbol to refer to.
     * @param ref Reference of to record.
     * @param reffile File where the reference is located.
     */
    void DiplomatIndex::add_reference_to(const slang::ast::Symbol& symbol, const slang::syntax::SyntaxNode& ref,const std::filesystem::path& reffile )
    {
        if(! is_registered(symbol))
            add_symbol(symbol);

        const slang::SourceManager* sm = symbol.getParentScope()->getCompilation().getSourceManager();
        
        _reference_table[&symbol].insert(&ref);
        _definition_table[&ref] = &symbol;

        Index_FileID_t rid = ensure_file(reffile);
        unsigned int ln = sm->getLineNumber(ref.sourceRange().start());
        if(! _index.at(rid).contains(ln))
            _index.at(rid)[ln] = {};
        _index.at(rid).at(ln).insert(&ref);


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
    bool DiplomatIndex::is_registered(const slang::syntax::SyntaxNode& node) const
    {
        return _definition_table.contains(&node);
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