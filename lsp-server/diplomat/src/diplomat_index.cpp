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
    void DiplomatIndex::_ensure_index(const Index_FileID_t &idx)
    {
        if(!_index.contains(idx))
        {
            _index[idx] = {};
            _definition_table[idx] = {};
            _reference_table[idx] = {};
        }
    }

    /**
     * @brief Ensure that the file exists in the index. 
     * Add the file if it does not exists, do nothing otherwise.
     * 
     * @param fileref Filepath to reference.
     */
    void DiplomatIndex::ensure_file(const fs::path& fileref)
    {
        _ensure_index(_index_from_filepath(fileref));
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

        if(!_reference_table.at(sid).contains(&symbol))
            _reference_table.at(sid)[&symbol] = {};
    }

    void DiplomatIndex::add_reference_to(const slang::ast::Symbol& symbol, const slang::syntax::SyntaxNode& ref,const std::filesystem::path& reffile )
    {

    }

    bool DiplomatIndex::is_registered(const slang::ast::Symbol& elt, std::optional<fs::path> file)
    {
        if(! file)
        {
            for(const auto& [file, _ ] : _index)
            {
                if(is_registered(elt,file))
                    return true;
            }
            return false;
        }
        else
        {
            if( _reference_table[file.value()].contains(&elt))
                return true;
            return false;
        }
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