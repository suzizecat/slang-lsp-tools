#pragma once

#include "types/structs/PublishDiagnosticsParams.hpp"
#include "slang/ast/Compilation.h"
#include "slang/diagnostics/DiagnosticClient.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "sv_document.hpp"
#include <unordered_map>
#include <memory>
#include <string>

namespace slsp
{

    typedef std::unordered_map<std::string, std::unique_ptr<SVDocument>> sv_doclist_t;
    class LSPDiagnosticClient : public slang::DiagnosticClient
    {
        
        const sv_doclist_t&  _documents;
        std::unordered_map<std::string, slsp::types::PublishDiagnosticsParams*> _diagnostics;


        public:
        LSPDiagnosticClient(const sv_doclist_t& doc_list);
        void _clear_diagnostics();
        void _cleanup_diagnostics();
        virtual void report(const slang::ReportedDiagnostic& diagnostic);

        inline const std::unordered_map<std::string, slsp::types::PublishDiagnosticsParams*>& get_publish_requests() const {return _diagnostics; };
    };

}