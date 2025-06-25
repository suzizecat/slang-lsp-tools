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
#include <filesystem>

#include "nlohmann/json.hpp"

#include "ast_print.hpp"
#include "index_visitor.hpp"
#include "index_reference_visitor.hpp"
#include "index_core.hpp"

#ifndef DIPLOMAT_VERSION_STRING
#define DIPLOMAT_VERSION_STRING "custom-build"
#endif

using json = nlohmann::json;

using namespace slang;
using namespace slang::driver;


int main(int argc, char** argv) {
    Driver driver;
    driver.addStandardArgs();

    std::optional<bool> showHelp;
    std::optional<bool> showVersion;
    std::optional<bool> verbose;
    std::optional<bool> trace;
    std::optional<std::string> cst_dump_file;
    std::optional<std::string> output_file;
    driver.cmdLine.add("-h,--help", showHelp, "Display available options");
    driver.cmdLine.add("--version", showVersion, "Display version information and exit");
    driver.cmdLine.add("-o,--output",output_file, "Output file for the index");
    driver.cmdLine.add("--verbose",verbose, "Enable verbose mode");
    driver.cmdLine.add("--trace",trace, "Enable verbosier mode");
    driver.cmdLine.add("--cst",cst_dump_file, "Dump the CST of the provided file, if found");

    if (!driver.parseCommandLine(argc, argv))
        return 1;

    if (showHelp == true) {
        printf("%s\n", driver.cmdLine.getHelpText("slang SystemVerilog compiler").c_str());
        return 0;
    }

    if (showVersion)
    {
        std::cout << fmt::format("Diplomat Indexer version {}",DIPLOMAT_VERSION_STRING) << std::endl;
        return 0;
    }

    if(verbose.value_or(false))
    {
        spdlog::set_level(spdlog::level::debug);
    }

    if(trace.value_or(false))
    {
        spdlog::set_level(spdlog::level::trace);
    }
   
    // spdlog::set_pattern("[%Y-%m-%d %T.%e] [%^%-5!l%$] %v");
    spdlog::set_pattern("[%^%-5!l%$] %v");
   
   

    if (!driver.processOptions())
        return 2;

    bool ok = driver.parseAllSources();

    auto compilation = driver.createCompilation();
        
    ok &= driver.reportCompilation(*compilation, /* quiet */ false);
    const ast::RootSymbol&  root_symb = compilation->getRoot();

    auto definitions = compilation->getDefinitions();
    

    diplomat::index::IndexVisitor indexer(compilation->getSourceManager());
    size_t failed_refs = 0;
    spdlog::stopwatch sw;

    root_symb.visit(indexer);
   
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

            if(cst_dump_file)
            {
                if(file->get_path() == std::filesystem::weakly_canonical(cst_dump_file.value()))
                {
                    print_slang_cst(stx);
                    return 0;
                }
            }

            
        }
    }

    spdlog::info("Analysis done in {:.6} !",sw);

    // Need to re-run because the files are filled out of order dur to the use of the syntax node.
    for(const auto& file : index->get_indexed_files())
        failed_refs += file->_get_nb_failed_refs();

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
    
    spdlog::info("Got {} failed references total.",failed_refs);
    return ok ? 0 : 3;
}