#pragma once

#include <exception>
#include <stdexcept>

namespace diplomat::index 
{
	class index_exception : public std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	/**
	 * @brief Error used when an index node is expected to be an orphan
	 * but is actually not.
	 * 
	 */
	class not_orphan_node_error : public  index_exception {
		using index_exception::index_exception;
	};
}