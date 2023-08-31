#include "diagnostic_client.hpp"

#include <filesystem>

#include "fmt/format.h"
#include "spdlog/spdlog.h"

using namespace slsp::types;
namespace slsp
{
    LSPDiagnosticClient::LSPDiagnosticClient(const sv_doclist_t &doc_list) : _documents(doc_list)
    {
    }

    void LSPDiagnosticClient::_clear_diagnostics()
    {
        for (auto [key, value] : _diagnostics)
            value->diagnostics.clear();
    }

    void LSPDiagnosticClient::_cleanup_diagnostics()
    {
        std::erase_if(_diagnostics, [](const auto &item)
                      {
        auto const& [key, value] = item;
        return value->diagnostics.size() == 0; });
    }

    void LSPDiagnosticClient::report(const slang::ReportedDiagnostic &to_report)
    {
        if (to_report.originalDiagnostic.code.getSubsystem() != slang::DiagSubsystem::General || to_report.originalDiagnostic.code.getCode() < slang::diag::NoteAssignedHere.getCode())
        {
            report_new_diagnostic(to_report);
        }
        else
        {
            report_related_diagnostic(to_report);
        }
    }

    void LSPDiagnosticClient::report_new_diagnostic(const slang::ReportedDiagnostic &to_report)
    {
        spdlog::debug("Report new diagnostic [{:3d}-{}] : {} ", to_report.originalDiagnostic.code.getCode(), slang::toString(to_report.originalDiagnostic.code), to_report.formattedMessage);
        if (to_report.location.bufferName.empty())
        {
            spdlog::warn("Diagnostic without buffer : [{:3d}-{}] : {}", to_report.originalDiagnostic.code.getCode(), slang::toString(to_report.originalDiagnostic.code), to_report.formattedMessage);
            _last_publication = nullptr;
            return;
        }
        std::filesystem::path buffer_path = std::filesystem::canonical(to_report.location.bufferName);
        SVDocument *doc = _documents.at(buffer_path.generic_string()).get();

        Diagnostic diag;
        diag.code = fmt::format("{}.{} {}",
                                (uint16_t)(to_report.originalDiagnostic.code.getSubsystem()),
                                to_report.originalDiagnostic.code.getCode(),
                                slang::toString(to_report.originalDiagnostic.code));

        diag.source = "diplomat-slang";
        diag.message = to_report.formattedMessage;
        diag.range = doc->range_from_slang(to_report.location, to_report.location);

        switch (to_report.severity)
        {
        case slang::DiagnosticSeverity::Ignored:
            // Ignore the diagnostic
            return;
            break;
        case slang::DiagnosticSeverity::Note:
            diag.severity = DiagnosticSeverity::DiagnosticSeverity_Information;
            break;
        case slang::DiagnosticSeverity::Warning:
            diag.severity = DiagnosticSeverity::DiagnosticSeverity_Warning;
            break;
        case slang::DiagnosticSeverity::Error:
        case slang::DiagnosticSeverity::Fatal:
            diag.severity = DiagnosticSeverity::DiagnosticSeverity_Error;
            break;
        default:
            diag.severity = DiagnosticSeverity::DiagnosticSeverity_Hint;
            break;
        }

        PublishDiagnosticsParams *pub;
        std::string the_uri = "file://" + buffer_path.generic_string();

        if (!_diagnostics.contains(the_uri))
        {
            pub = new PublishDiagnosticsParams();
            pub->uri = the_uri;
            _diagnostics[the_uri] = pub;
        }
        else
        {
            pub = _diagnostics[the_uri];
        }

        pub->diagnostics.push_back(diag);
        _last_publication = pub;
    }

    void LSPDiagnosticClient::report_related_diagnostic(const slang::ReportedDiagnostic &to_report)
    {
        spdlog::info("    add to diagnostic [{:3d}] : {} ", to_report.originalDiagnostic.code.getCode(), to_report.formattedMessage);
        if (_last_publication == nullptr || _last_publication->diagnostics.size() == 0)
        {
            spdlog::error("Got a follow-up diagnostic without previous diagnostic. Ignore.");
        }
        else
        {

            std::filesystem::path buffer_path = std::filesystem::canonical(to_report.location.bufferName);
            SVDocument *doc = _documents.at(buffer_path.generic_string()).get();
            std::string the_uri = "file://" + buffer_path.generic_string();

            // In some case, slang report "first defined here" kind of diagnostics.
            // In this situation, we add it as related information.
            DiagnosticRelatedInformation rel_info;
            rel_info.location.range = doc->range_from_slang(to_report.location, to_report.location);
            rel_info.location.uri = the_uri;
            rel_info.message = to_report.formattedMessage;

            // It is assumed that the original report already exists and was the last one emitted.
            Diagnostic &orig_diag = _last_publication->diagnostics.back();

            if (!orig_diag.relatedInformation)
            {
                orig_diag.relatedInformation = {rel_info};
            }
            else
            {
                orig_diag.relatedInformation.value().push_back(rel_info);
            }
        }
    }

} // namespace slsp
