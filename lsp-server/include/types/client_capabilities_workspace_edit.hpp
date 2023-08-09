#pragma once

#include <optional>

#include "enums.hpp"

namespace slsp{
namespace type{
    

    typedef struct _changeAnnotationSupport{
        /**
         * Whether the client groups edits with equal labels into tree nodes,
         * for instance all edits labelled with "Changes in Strings" would
         * be a tree node.
         */
        std::optional<bool> groupsOnLabel;
    } _changeAnnotationSupport;

    typedef struct WorkspaceEditClientCapabilities {
        /**
         * The client supports versioned document changes in `WorkspaceEdit`s
         */
		std::optional<bool> documentChange;


        /**
         * The resource operations the client supports. Clients should at least
         * support 'create', 'rename' and 'delete' files and folders.
         *
         * @since 3.13.0
         */
        std::optional<std::vector<RessourceOperationKind> > resourceOperations;

        /**
         * The failure handling strategy of a client if applying the workspace edit
         * fails.
         *
         * @since 3.13.0
         */
        std::optional<FailureHandlingKind> failureHandling;

        /**
         * Whether the client normalizes line endings to the client specific
         * setting.
         * If set to `true` the client will normalize line ending characters
         * in a workspace edit to the client specific new line character(s).
         *
         * @since 3.16.0
         */
        std::optional<bool> normalizesLineEndings;

        /**
         * Whether the client in general supports change annotations on text edits,
         * create file, rename file and delete file changes.
         *
         * @since 3.16.0
         */
        std::optional<_changeAnnotationSupport> changeAnnotationSupport;

    } WorkspaceEditClientCapabilities;
} // namespace type
} // namespace slsp
