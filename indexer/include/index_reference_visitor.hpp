#pragma once

#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/parsing/Token.h>

#include <memory>

#include "index_core.hpp"

namespace diplomat::index
{
	class IndexCore;

  class ReferenceVisitor : public slang::syntax::SyntaxVisitor<ReferenceVisitor>
  {
		const slang::SourceManager* _sm;
		IndexCore* _index;
		
		bool _add_reference_from_stx(const slang::syntax::SyntaxNode&, const std::string_view& name);
	public :
		explicit ReferenceVisitor(const slang::SourceManager* sm, IndexCore* idx) : _sm(sm), _index(idx) {};

		    // void handle(const slang::syntax::ModuleHeaderSyntax& node);
            // void handle(const slang::syntax::HierarchyInstantiationSyntax& node);
            void handle(const slang::syntax::IdentifierNameSyntax& node);
            // void handle(const slang::syntax::IdentifierSelectNameSyntax& node);
            // void handle(const slang::syntax::ElementSelectSyntax& node);
            // void handle(const slang::syntax::NamedPortConnectionSyntax& node);
            // void handle(const slang::syntax::NamedParamAssignmentSyntax& node);

  };

}