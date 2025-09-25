#pragma once

#include <nlohmann/json.hpp>
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/ValueSymbol.h>
#include <slang/ast/ASTVisitor.h>
#include <string>
#include <unordered_map>
#include <filesystem>

#include "uri.hh"
#include "diplomat_document_cache.hpp"

class HierVisitor : public slang::ast::ASTVisitor<HierVisitor,false,false>
{
    nlohmann::json_pointer<std::string> _pointer;
    nlohmann::json _hierarchy;
    const diplomat::cache::DiplomatDocumentCache* _cache;
    // const std::unordered_map<std::filesystem::path, uri>*  _doc_path_to_uri;
    bool _output_io;
    void _create_instance(const std::string& name);
    public : 
    //explicit HierVisitor(bool output_io = true, const std::unordered_map<std::filesystem::path, uri>* path_to_uri = nullptr);
    explicit HierVisitor(bool output_io = true, const diplomat::cache::DiplomatDocumentCache* cache = nullptr);
    void handle(const slang::ast::InstanceSymbol& node);
    void handle(const slang::ast::PortSymbol& node);
    void handle(const slang::ast::UninstantiatedDefSymbol& node);
    //void handle(const slang::ast::ValueSymbol& node);

    /**
     * @brief Return the hierarchy object
     * 
     * @return const nlohmann::json& the json object representing the hierarchy.
     *
     * @todo Provide a metamodel as an exchange type of the hierarchy, and update the
     * underlying code.
     */
    inline const nlohmann::json& get_hierarchy() const {return _hierarchy;}; 
};