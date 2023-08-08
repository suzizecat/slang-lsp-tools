#pragma once

#include <string>
#include "uri.hh"


namespace slsp{
namespace type{
	typedef struct WorkspaceFolder {
		uri uri;
		std::string name;
	} WorkspaceFolder_t;
} // namespace type
} // namespace slsp
