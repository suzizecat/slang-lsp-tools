#pragma once

#include <optional>

#include "client_capabilities_workspace_edit.hpp"

namespace slsp{
namespace type{
    
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
		std::optional<WorkspaceEditClientCapabilities_t> workspaceEdit;
	} _ClientCapabilities_workspace_t;
} // namespace type
} // namespace slsp
