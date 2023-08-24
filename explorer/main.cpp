#include "slang/driver/Driver.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/Scope.h"
#include "slang/ast/Symbol.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/Definition.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/util/Version.h"
#include "slang/util/Enum.h"
#include "slang/text/SourceLocation.h"
#include <iostream>
#include "explorer_visitor.h"

using namespace slang;
using namespace slang::driver;

void print_symbol_origin(const ast::Scope& scope, const ast::Symbol& symb)
{
    const auto* stx_node = symb.getSyntax();
    const slang::SourceManager* sm = scope.getCompilation().getSourceManager();
    if (stx_node != nullptr)
    {
        slang::SourceRange pos = stx_node->sourceRange();
        std::cout << sm->getFileName(pos.start()) << ":" << sm->getLineNumber(pos.start()) << ":" << sm->getColumnNumber(pos.start());
        std::cout                                 << ":" << sm->getLineNumber(pos.end()) << ":" << sm->getColumnNumber(pos.end());
    }
}

const syntax::SyntaxNode* getNodeFromPosition(const syntax::SyntaxNode* root, const SourceLocation& pos)
{
    if(root == nullptr)
        return nullptr;
    
    const slang::SourceLocation root_start = root->sourceRange().start();
    const slang::SourceLocation root_end = root->sourceRange().end();
    
    if(root->sourceRange().start().buffer() != pos.buffer())
        return nullptr;
    if(root_start > pos || pos > root_end)
        return nullptr;
    if(root->getChildCount() == 0)
        return root;

    const syntax::SyntaxNode* ret = nullptr;
    for(int i = 0; i< root->getChildCount(); i++)
    {
        ret = getNodeFromPosition(root->childNode(i),pos);
        if(ret != nullptr)
            return ret;
    }
    return root;
}

void print_symbols(const ast::Scope& scope, int level = 0)
{
    const ast::Symbol* result = scope.lookupName("is_diff");
    if(result != nullptr)
    {
        for(int i = 0; i < level ; i++)
            std::cout << "  ";
        std::cout << " $ Ref found to ";
        print_symbol_origin(scope,*result);
        std::cout << std::endl;
    }


    for (const ast::Symbol& symb : scope.members())
    {
        for(int i = 0; i < level; i++)
            std::cout << "  ";
        std::cout << (symb.isScope() ? " # Scope " : " - Symbol")  << " of kind " << toString(symb.kind) << " with name " << symb.name << " ";


        if(symb.isScope())
        {
            std::cout << std::endl;            
            print_symbols(symb.as<ast::Scope>(),level +1);
        }
        else if (symb.kind == ast::SymbolKind::Instance)
        {
            std::cout << std::endl;
            print_symbols(symb.as<ast::InstanceSymbol>().body, level +1);
        }
        else
        {
            const auto* stx_node = symb.getSyntax();
            const slang::SourceManager* sm = scope.getCompilation().getSourceManager();
            if (stx_node != nullptr)
            {
                slang::SourceRange pos = stx_node->sourceRange();
                std::cout << sm->getFileName(pos.start()) << ":" << sm->getLineNumber(pos.start()) << ":" << sm->getColumnNumber(pos.start());
                std::cout                                 << ":" << sm->getLineNumber(pos.end()) << ":" << sm->getColumnNumber(pos.end());
            }
            std::cout << std::endl;
            // const auto* definition = symb.;
            // if(definition != nullptr)
            // {
            //     for(int i = 0; i < level+2; i++)
            //         std::cout << "  ";
            //     std::cout << "-> " ;
            //     slang::SourceRange pos = definition->syntax.sourceRange();
            //     std::cout << sm->getFileName(pos.start()) << ":" << sm->getLineNumber(pos.start()) << ":" << sm->getColumnNumber(pos.start());
            //     std::cout                                 << ":" << sm->getLineNumber(pos.end()) << ":" << sm->getColumnNumber(pos.end());
            //     std::cout << std::endl;
            // }
        }
        
        
    }
}


int main(int argc, char** argv) {
    Driver driver;
    driver.addStandardArgs();

    std::optional<bool> showHelp;
    std::optional<bool> showVersion;
    driver.cmdLine.add("-h,--help", showHelp, "Display available options");
    driver.cmdLine.add("--version", showVersion, "Display version information and exit");

    if (!driver.parseCommandLine(argc, argv))
        return 1;

    if (showHelp == true) {
        printf("%s\n", driver.cmdLine.getHelpText("slang SystemVerilog compiler").c_str());
        return 0;
    }

    if (showVersion == true) {
        printf("slang version %d.%d.%d+%s\n", VersionInfo::getMajor(),
            VersionInfo::getMinor(), VersionInfo::getPatch(),
            std::string(VersionInfo::getHash()).c_str());
        return 0;
    }

    if (!driver.processOptions())
        return 2;

    bool ok = driver.parseAllSources();

    auto compilation = driver.createCompilation();
        
    ok &= driver.reportCompilation(*compilation, /* quiet */ false);
    const ast::RootSymbol&  root_symb = compilation->getRoot();
    const ast::Symbol* net = root_symb.lookupName("K_DWIDTH");
    SVvisitor visitor;
    
    if(compilation->isFinalized())
    {
        std::cout << "Design has been compiled properly" << std::endl;
    }

    print_symbols(root_symb);


    std::cout << "Testing global lookup..." << std::endl;
    const ast::Symbol* results = root_symb.lookupName("is_diff");
    if(results != nullptr)
        std::cout << "    Found the requested symbol somewhere !" << std::endl;
    else
        std::cout << "    Nothing found." << std::endl;
    for(std::shared_ptr<syntax::SyntaxTree> stree : driver.syntaxTrees)
    {
        std::cout << "Found a syntax tree" << std::endl;
        const syntax::SyntaxNode& root = stree->root();
        root.visit(visitor);
        const syntax::SyntaxNode* lu_node = getNodeFromPosition(&root,slang::SourceLocation(stree->root().sourceRange().start().buffer(),390));
        if(lu_node != nullptr)
            std::cout << "Found: " << lu_node->toString() << std::endl;
        //std::cout << stree->root().toString() << std::endl;
    }
    
    
    return ok ? 0 : 3;
}