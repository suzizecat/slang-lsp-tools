#include "slang/ast/Compilation.h"
#include "slang/driver/Driver.h"
#include "slang/util/Version.h"

#include <iostream>

#include "documenter_visitor.h"

using namespace slang;
using namespace slang::driver;

int main(int argc, char** argv) {
    Driver driver;
    driver.addStandardArgs();

    std::optional<bool> showHelp;
    std::optional<bool> showVersion;
    std::optional<bool> pretty;
    std::optional<bool> onlyModules;
    driver.cmdLine.add("-h,--help", showHelp, "Display available options");
    driver.cmdLine.add("--version", showVersion, "Display version information and exit");
    driver.cmdLine.add("--pretty", pretty, "Prettify the output");
    driver.cmdLine.add("--only-modules", onlyModules, "Only output module names");

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
    ok &= driver.reportCompilation(*compilation, /* quiet */ true);

    auto syntax_trees = compilation->getSyntaxTrees();
   
   
    DocumenterVisitor visitor(bool(onlyModules), &(driver.sourceManager));
    json output;
    
    output["modules"] = json::array();
    if(! onlyModules)
        output["content"] = json::array();
    for (auto st : syntax_trees)
    {
        //std::cout << "Found syntax tree" << std::endl;
        visitor.set_source_manager(&(st->sourceManager()));
        st->root().visit(visitor);
        output["modules"].push_back(visitor.module);
        if(! onlyModules)
            output["content"].push_back(visitor.data);
    }

    //std::cout << "JSON Output is :" << std::endl;
    if(pretty)
        std::cout << output.dump(4) << std::endl;
    else
        std::cout << output.dump() << std::endl;

    return ok ? 0 : 3;
}