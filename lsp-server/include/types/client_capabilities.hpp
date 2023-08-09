#pragma once

 #include <optional>

 #include "client_capabilities_workspace.hpp"

namespace slsp{
namespace type{
    typedef struct ClientCapabilities
	{
		/**
		 * Workspace specific client capabilities.
		 */
		std::optional<_ClientCapabilities_workspace_t> workspace;

		
	} ClientCapabilities_t;
} // namespace type
} // namespace slsp
