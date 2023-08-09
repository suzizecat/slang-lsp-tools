#pragma once

#include "nlohmann/json.hpp"

namespace slsp {
namespace type {
    
    typedef enum class TraceValue {Off, Messages, Verbose, _Invalid=-1} TraceValue;

	NLOHMANN_JSON_SERIALIZE_ENUM(TraceValue,{
		{_Invalid, nullptr},
		{ Off, "off"},
		{Messages, "messages"},
		{Verbose, "verbose"},
	})

	typedef enum class RessourceOperationKind {Create,Rename,Delete, _Invalid=-1} RessourceOperationKind;

	NLOHMANN_JSON_SERIALIZE_ENUM(RessourceOperationKind,{
		{_Invalid, nullptr},
		{Create,"create"},
		{Rename,"rename"},
		{Delete,"delete"},
	})

	typedef enum class FailureHandlingKind {Abort,Transactional,Undo,TextOnlyTransactional, _Invalid=-1} FailureHandlingKind;
	NLOHMANN_JSON_SERIALIZE_ENUM(FailureHandlingKind,{
		{_Invalid, nullptr},
		{Abort,"abort"},
		{Transactional,"transactional"},
		{Undo,"undo"},
		{TextOnlyTransactional,"textOnlyTransactional"},
	})

	/**
	 * A symbol kind.
	 */
	typedef enum class SymbolKind 
	{
		File = 1,
		Module = 2,
		Namespace = 3,
		Package = 4,
		Class = 5,
		Method = 6,
		Property = 7,
		Field = 8,
		Constructor = 9,
		Enum = 10,
		Interface = 11,
		Function = 12,
		Variable = 13,
		Constant = 14,
		String = 15,
		Number = 16,
		Boolean = 17,
		Array = 18,
		Object = 19,
		Key = 20,
		Null = 21,
		EnumMember = 22,
		Struct = 23,
		Event = 24,
		Operator = 25,
		TypeParameter = 26
	} SymbolKind;

	/**
	 * Symbol tags are extra annotations that tweak the rendering of a symbol.
	 *
	 * @since 3.16
	 */
	typedef enum class SymbolTag {
		/**
		 * Render a symbol as obsolete, usually using a strike-out.
		 */
		Deprecated = 1
	} SymbolTag;
}
}