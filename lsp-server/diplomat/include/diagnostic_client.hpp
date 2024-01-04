#pragma once

#include "types/structs/PublishDiagnosticsParams.hpp"
#include "slang/ast/Compilation.h"
#include "slang/diagnostics/DiagnosticClient.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "sv_document.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <filesystem>

namespace slsp
{

    typedef std::unordered_map<std::filesystem::path, std::unique_ptr<SVDocument>> sv_doclist_t;
    class LSPDiagnosticClient : public slang::DiagnosticClient
    {
         protected:
            const sv_doclist_t&  _documents;
            std::unordered_map<std::string, std::unique_ptr<slsp::types::PublishDiagnosticsParams> > _diagnostics;
            slsp::types::PublishDiagnosticsParams* _last_publication;

            void _remap_internal_diagnostic_uri(slsp::types::PublishDiagnosticsParams* diag, const std::string& old_uri, const std::string& new_uri);
        public:
            LSPDiagnosticClient(const sv_doclist_t& doc_list);
            void _clear_diagnostics();
            void _cleanup_diagnostics();
            virtual void report(const slang::ReportedDiagnostic& to_report);

            void report_new_diagnostic(const slang::ReportedDiagnostic& to_report);
            void report_related_diagnostic(const slang::ReportedDiagnostic& to_report);
            bool remap_diagnostic_uri(const std::string& orig_uri, const std::string& new_uri);
            inline const std::unordered_map<std::string, std::unique_ptr<slsp::types::PublishDiagnosticsParams> >& get_publish_requests() const {return _diagnostics; };
    };

}