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
        for (auto& [key, value] : _diagnostics)
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
        if (to_report.location.bufferName.empty())
        {
            spdlog::warn("Diagnostic without buffer : [{:3d}-{}] : {}", to_report.originalDiagnostic.code.getCode(), slang::toString(to_report.originalDiagnostic.code), to_report.formattedMessage);
            _last_publication = nullptr;
            return;
        }
        spdlog::debug("Report new diagnostic [{:3d}-{}] : {} ({}) ", to_report.originalDiagnostic.code.getCode(), slang::toString(to_report.originalDiagnostic.code), to_report.formattedMessage,  to_report.location.bufferName);
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
        // Absolute will not cleanup once it reach a symlink, so it is required to use canonical
        // Simple concatenation does not work as of C++23 ...
        std::string the_uri = doc->doc_uri.value_or(fmt::format("file://{}",buffer_path.generic_string()));
        
        if (!_diagnostics.contains(the_uri))
        {
            pub = new PublishDiagnosticsParams();
            pub->uri = the_uri;
            _diagnostics[the_uri] = std::unique_ptr<PublishDiagnosticsParams>(pub);
        }
        else
        {
            pub = _diagnostics[the_uri].get();
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

            std::string the_uri = doc->doc_uri.value_or(fmt::format("file://{}",buffer_path.generic_string()));

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

    /**
     * @brief Rebinds all diagnostic references of a given URI to another URI.
     * 
     * @details
     *  - The rebinding is done without reallocation.
     *  - The deleted URI reference is then re-inserted as an empty diagnostic set 
     * to be cleaned up afterwards.
     *  - It is especially useful when dealing with symlink that can throw off the client.
     * 
     * @param orig_uri URI to look up
     * @param new_uri URI to set as a replacement
     * 
     * @return true if at least one replacement has been done
     */
    bool LSPDiagnosticClient::remap_diagnostic_uri(const std::string& orig_uri, const std::string& new_uri)
    {
        if(_diagnostics.contains(orig_uri))
        {
            auto record = _diagnostics.extract(orig_uri);
            record.key() = new_uri;
            _diagnostics.insert(std::move(record));           

            for(auto& diag : _diagnostics)
            {
                _remap_internal_diagnostic_uri(diag.second.get(),orig_uri,new_uri);
            }

            // Used to trigger diagnostic deletion on client side
            std::unique_ptr<PublishDiagnosticsParams> placeholder = std::make_unique<PublishDiagnosticsParams>();
            placeholder->uri = orig_uri;
            _diagnostics[orig_uri] = std::move(placeholder);

            return true;
        }
        return false;
    }


    /**
     * @brief Update the URI values matching a given URI within a diagnostic set.
     * 
     * @see LSPDiagnosticClient::remap_diagnostic_uri
     * 
     * @param diag Diagnostic set to update
     * @param old_uri URI to look up
     * @param new_uri New URI value
     */
    void  LSPDiagnosticClient::_remap_internal_diagnostic_uri(PublishDiagnosticsParams* diag, const std::string& old_uri, const std::string& new_uri)
    {
        if(diag->uri == old_uri)
            diag->uri = new_uri;

        for(auto& d : diag->diagnostics)
        {
            if(d.relatedInformation)
            {
                for(auto& related : d.relatedInformation.value())
                {
                    if(related.location.uri == old_uri)
                    {
                        related.location.uri = new_uri;
                    }
                }
            }

        }
    }

} // namespace slsp
