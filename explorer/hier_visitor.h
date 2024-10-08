#pragma once

#include <nlohmann/json.hpp>
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/ValueSymbol.h>
#include <slang/ast/ASTVisitor.h>
#include <string>
#include <map>
#include <filesystem>

class HierVisitor : public slang::ast::ASTVisitor<HierVisitor,false,false>
{
    nlohmann::json_pointer<std::string> _pointer;
    nlohmann::json _hierarchy;
    const std::unordered_map<std::filesystem::path, std::string>* _doc_path_to_uri;
    bool _output_io;
    void _create_instance(const std::string& name);
    public : 
    explicit HierVisitor(bool output_io = true, const std::unordered_map<std::filesystem::path, std::string>* path_to_uri = nullptr);
    void handle(const slang::ast::InstanceSymbol& node);
    void handle(const slang::ast::PortSymbol& node);
    void handle(const slang::ast::UninstantiatedDefSymbol& node);
    //void handle(const slang::ast::ValueSymbol& node);

    inline const nlohmann::json& get_hierarchy() const {return _hierarchy;}; 
};