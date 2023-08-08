#pragma once

#include <string>
#include <optional>

#include "uri.hh"
#include "nlohmann/json.hpp"

#include "client_info.hpp"
#include "enums.hpp"
#include "workspace_folder.hpp"
#include "client_capabilities.hpp"

namespace slsp {
namespace type {

	struct InitializationParams
	{
		/**
		 * The process Id of the parent process that started the server. Is null if
		 * the process has not been started by another process. If the parent
		 * process is not alive then the server should exit (see exit notification)
		 * its process.
		*/
		std::optional<int> processId;

		std::optional<clientInfo_t> clientInfo;

		/**
		 * The locale the client is currently showing the user interface
		 * in. This must not necessarily be the locale of the operating
		 * system.
		 *
		 * Uses IETF language tags as the value's syntax
		 * (See https://en.wikipedia.org/wiki/IETF_language_tag)
		 *
		 * @since 3.16.0
		 */
		std::optional<std::string> locale;

		/**
		 * The rootUri of the workspace. Is null if no
		 * folder is open. If both `rootPath` and `rootUri` are set
		 * `rootUri` wins.
		 *
		 * @deprecated in favour of `workspaceFolders`
		 */
		std::optional<uri> rootUri;

		/**
	 	 * User provided initialization options.
	 	 */
		nlohmann::json initializationOptions;

		/**
	 	 * The capabilities provided by the client (editor or tool)
	 	 */
		// capabilities: ClientCapabilities;

		/**
		 * The initial trace setting. If omitted trace is disabled ('off').
		 */
		std::optional<TraceValue_t> trace;

		/**
		 * The workspace folders configured in the client when the server starts.
		 * This property is only available if the client supports workspace folders.
		 * It can be `null` if the client supports workspace folders but none are
		 * configured.
		 *
		 * @since 3.6.0
		 */
		std::optional<std::vector<WorkspaceFolder_t>> workspaceFolders;

	};
}
}