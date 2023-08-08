#pragma once

#include "nlohmann/json.hpp"

namespace slsp {
namespace type {
    
    typedef enum class TraceValue {Off, Messages, Verbose, _Invalid=-1} TraceValue_t;

	NLOHMANN_JSON_SERIALIZE_ENUM(TraceValue,{
		{_Invalid, nullptr},
		{ Off, "off"},
		{Messages, "messages"},
		{Verbose, "verbose"},
	})

	typedef enum class RessourceOperationKind {Create,Rename,Delete, _Invalid=-1} RessourceOperationKind_t;

	NLOHMANN_JSON_SERIALIZE_ENUM(RessourceOperationKind,{
		{_Invalid, nullptr},
		{Create,"create"},
		{Rename,"rename"},
		{Delete,"delete"},
	})

	typedef enum class FailureHandlingKind {Abort,Transactional,Undo,TextOnlyTransactional, _Invalid=-1} FailureHandlingKind_t;
	NLOHMANN_JSON_SERIALIZE_ENUM(FailureHandlingKind,{
		{_Invalid, nullptr},
		{Abort,"abort"},
		{Transactional,"transactional"},
		{Undo,"undo"},
		{TextOnlyTransactional,"textOnlyTransactional"},
	})
}
}