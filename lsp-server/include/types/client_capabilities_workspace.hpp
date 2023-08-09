#pragma once

#include <optional>

#include "client_capabilities_workspace_edit.hpp"
#include "client_capabilities_workspace_symbol.hpp"

namespace slsp{
namespace type{
    
	typedef struct DidChangeConfigurationClientCapabilities {
		/**
		 * Did change configuration notification supports dynamic registration.
		 */
		std::optional<bool> dynamicRegistration;
	} DidChangeConfigurationClientCapabilities;

	typedef struct DidChangeWatchedFilesClientCapabilities {
		/**
		 * Did change watched files notification supports dynamic registration.
		 * Please note that the current protocol doesn't support static
		 * configuration for file changes from the server side.
		 */
		std::optional<bool> dynamicRegistration;

		/**
		 * Whether the client has support for relative patterns
		 * or not.
		 *
		 * @since 3.17.0
		 */
		std::optional<bool> relativePatternSupport;
	} DidChangeWatchedFilesClientCapabilities;

	typedef struct ExecuteCommandClientCapabilities {
	
	/**
	 * Execute command supports dynamic registration.
	 */
	std::optional<bool> dynamicRegistration;
	
	} ExecuteCommandClientCapabilities;
	

	typedef struct _ClientCapabilities_workspace {
		/**
		 * The client supports applying batch edits
		 * to the workspace by supporting the request
		 * 'workspace/applyEdit'
		 */
		std::optional<bool> applyEdit;

		/**
		 * Capabilities specific to `WorkspaceEdit`s
		 */
		std::optional<WorkspaceEditClientCapabilities> workspaceEdit;

		/**
		 * Capabilities specific to the `workspace/didChangeConfiguration`
		 * notification.
		 */
		std::optional<DidChangeConfigurationClientCapabilities> didChangeConfiguration;

		/**
		 * Capabilities specific to the `workspace/didChangeWatchedFiles`
		 * notification.
		 */
		std::optional<DidChangeWatchedFilesClientCapabilities> didChangeWatchedFiles;		

		/**
		 * Capabilities specific to the `workspace/symbol` request.
		 */
		std::optional<WorkspaceSymbolClientCapabilities> symbol;

		/**
		 * Capabilities specific to the `workspace/executeCommand` request.
		 */
		std::optional<ExecuteCommandClientCapabilities> executeCommand;

		/**
		 * The client has support for workspace folders.
		 *
		 * @since 3.6.0
		 */
		std::optional<bool> workspaceFolders;

		/**
		 * The client supports `workspace/configuration` requests.
		 *
		 * @since 3.6.0
		 */
		std::optional<bool> configuration;
		
		
	} _ClientCapabilities_workspace_t;
} // namespace type
} // namespace slsp
