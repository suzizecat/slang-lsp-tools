#include "diagnostic_client.hpp"

#include <filesystem>


#include "spdlog/spdlog.h"


using namespace slsp::types;
namespace slsp
{
    LSPDiagnosticClient::LSPDiagnosticClient(const sv_doclist_t& doc_list) :
        _documents(doc_list)
    {
    }

void LSPDiagnosticClient::_clear_diagnostics()
{
    for(auto [key,value] : _diagnostics)
        value->diagnostics.clear();
}

void LSPDiagnosticClient::_cleanup_diagnostics()
{
    std::erase_if(_diagnostics,[](const auto& item){
        auto const& [key, value] = item;
        return value->diagnostics.size() == 0;
    });
}

void LSPDiagnosticClient::report(const slang::ReportedDiagnostic& to_report)
{
    PublishDiagnosticsParams* pub;
    std::filesystem::path buffer_path = std::filesystem::canonical(to_report.location.bufferName);
    SVDocument* doc = _documents.at(buffer_path.generic_string()).get();

    Diagnostic diag;
    diag.code = slang::toString(to_report.originalDiagnostic.code);
    diag.source = "diplomat-slang";
    diag.message = to_report.formattedMessage;
    diag.range = doc->range_from_slang(to_report.location, to_report.location);

    spdlog::info("Report diagnostic {} with code {}",diag.message, to_report.originalDiagnostic.code.getCode());

    switch (to_report.severity)
    {
    case slang::DiagnosticSeverity::Ignored :
        // Ignore the diagnostic
        return;
        break;
    case slang::DiagnosticSeverity::Note:
        diag.severity = DiagnosticSeverity::DiagnosticSeverity_Information;
        break ;
    case slang::DiagnosticSeverity::Warning :
        diag.severity = DiagnosticSeverity::DiagnosticSeverity_Warning;
        break;
    case slang::DiagnosticSeverity::Error :
    case slang::DiagnosticSeverity::Fatal :
        diag.severity = DiagnosticSeverity::DiagnosticSeverity_Error;
        break;
    default:
        diag.severity = DiagnosticSeverity::DiagnosticSeverity_Hint;
        break;
    }

    
    std::string the_uri = "file://" + buffer_path.generic_string();
    if(!_diagnostics.contains(the_uri))
    {
        pub = new PublishDiagnosticsParams();
        pub->uri = the_uri;
        _diagnostics[the_uri] = pub;
    }
    else
    {
        pub = _diagnostics[the_uri];
    }  

    if(to_report.originalDiagnostic.code.getSubsystem() != slang::DiagSubsystem::General 
    || to_report.originalDiagnostic.code.getCode() < slang::diag::NoteAssignedHere.getCode())
    {
        pub->diagnostics.push_back(diag);
    }
    else
    {
        // In some case, slang report "first defined here" kind of diagnostics.
        // In this situation, we add it as related information.
        DiagnosticRelatedInformation rel_info;
        rel_info.location.range = diag.range;
        rel_info.location.uri = the_uri;
        rel_info.message = diag.message;
       
        // It is assumed that the original report already exists and was the last one emitted.
        Diagnostic& orig_diag = pub->diagnostics.back();

        if(! orig_diag.relatedInformation)
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
