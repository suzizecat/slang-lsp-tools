#pragma once

#include <optional>
#include <vector>
#include <string>

#include "enums.hpp" 
namespace slsp{
namespace type{

    typedef struct _SymbolKindCapabilities
    {
        /**
		 * The symbol kind values the client supports. When this
		 * property exists the client also guarantees that it will
		 * handle values outside its set gracefully and falls back
		 * to a default value when unknown.
		 *
		 * If this property is not present the client only supports
		 * the symbol kinds from `File` to `Array` as defined in
		 * the initial version of the protocol.
		 */
		std::optional<std::vector<SymbolKind> > valueSet;
        
    } _SymbolKindCapabilities;

    
    typedef struct _SymbolTagCapabilities
    {
        /**
		 * The tags supported by the client.
		 */
        std::optional<std::vector<SymbolTag> > valueSet;
        
    } _SymbolTagCapabilities;


    typedef struct _ResolveSupportCapabilities
    {
        /**
		 * The properties that a client can resolve lazily. Usually
		 * `location.range`
		 */
        std::optional<std::vector<std::string> > properties;
        
    } _ResolveSupportCapabilities;


    typedef struct WorkspaceSymbolClientCapabilities
    {
        /**
         * Symbol request supports dynamic registration.
         */
        std::optional<bool> dynamicRegistration;

        /**
         * Specific capabilities for the `SymbolKind` in the `workspace/symbol`
         * request.
         */
        std::optional<_SymbolKindCapabilities> symbolKind;

        /**
         * The client supports tags on `SymbolInformation` and `WorkspaceSymbol`.
         * Clients supporting tags have to handle unknown tags gracefully.
         *
         * @since 3.16.0
         */
        std::optional<_SymbolTagCapabilities> tagSupport;
        
        /**
         * The client support partial workspace symbols. The client will send the
         * request `workspaceSymbol/resolve` to the server to resolve additional
         * properties.
         *
         * @since 3.17.0 - proposedState
         */
        std::optional<_ResolveSupportCapabilities> resolveSupport;
        
    };
    
} // namespace type
} // namespace slsp
