#include "slang/driver/Driver.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/Scope.h"
#include "slang/ast/Symbol.h"

#include <iostream>
#include <fstream>
#include <stack>
#include "spdlog/spdlog.h"
#include "spdlog/stopwatch.h"
#include "diplomat_index.hpp"
#include "visitor_index.hpp"
#include "nlohmann/json.hpp"


#include "index_visitor.hpp"
#include "index_reference_visitor.hpp"
#include "index_core.hpp"

using json = nlohmann::json;

using namespace slang;
using namespace slang::driver;


int main(int argc, char** argv) {
    Driver driver;
    driver.addStandardArgs();

    std::optional<bool> showHelp;
    std::optional<bool> showVersion;
    std::optional<std::string> output_file;
    driver.cmdLine.add("-h,--help", showHelp, "Display available options");
    driver.cmdLine.add("--version", showVersion, "Display version information and exit");
    driver.cmdLine.add("-o,--output",output_file, "Output file for the index");

    if (!driver.parseCommandLine(argc, argv))
        return 1;

    if (showHelp == true) {
        printf("%s\n", driver.cmdLine.getHelpText("slang SystemVerilog compiler").c_str());
        return 0;
    }

   

    if (!driver.processOptions())
        return 2;

    bool ok = driver.parseAllSources();

    auto compilation = driver.createCompilation();
        
    ok &= driver.reportCompilation(*compilation, /* quiet */ false);
    const ast::RootSymbol&  root_symb = compilation->getRoot();

    auto definitions = compilation->getDefinitions();
    
    // if(compilation->isFinalized())
    // {
    //     std::cout << "Design has been compiled properly" << std::endl;
    // }
    
    /*
    slsp::IndexVisitor indexer(compilation->getSourceManager());
    
    /*/
    diplomat::index::IndexVisitor indexer(compilation->getSourceManager());
    // */
    //root_symb.visit(vis);

    spdlog::stopwatch sw;

    root_symb.visit(indexer);
    /*
    slsp::DiplomatIndex* index = indexer.get_index();
    /*/
    std::unique_ptr<diplomat::index::IndexCore> index = std::move(indexer.get_index());

    for(const auto& file : index->get_indexed_files())
    {
        {
            spdlog::info("Processing references for {}",file->get_path().generic_string());

            auto stx = file->get_syntax_root();
            if(stx)
            {
                diplomat::index::ReferenceVisitor ref_visitor(compilation->getSourceManager(),index.get());
                stx->visit(ref_visitor);
            }
        }
    }
    //*/
    spdlog::info("Analysis done in {:.6} !",sw);

    if(output_file)
    {
        std::ofstream dump_file;
        dump_file.open(output_file.value());
        spdlog::info("Start dumping index to {}", output_file.value());
        dump_file << index->dump().dump(4);
        spdlog::info("Done.");
        dump_file.close();
    }
    else
        std::cout << index->dump().dump(4);   
    return ok ? 0 : 3;
}