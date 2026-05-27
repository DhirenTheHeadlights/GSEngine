//===--- ConceptInTemplateParamCheck.cpp - clang-tidy ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ConceptInTemplateParamCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprConcepts.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

namespace {

const TemplateTypeParmDecl *singleTypeParam(const TemplateParameterList *TPL) {
  if (!TPL || TPL->size() != 1)
    return nullptr;
  return dyn_cast<TemplateTypeParmDecl>(TPL->getParam(0));
}

const ConceptSpecializationExpr *
asSingleConceptOnParam(const Expr *RequiresClause,
                       const TemplateTypeParmDecl *Param) {
  if (!RequiresClause || !Param)
    return nullptr;
  const auto *CSE =
      dyn_cast<ConceptSpecializationExpr>(RequiresClause->IgnoreParenImpCasts());
  if (!CSE)
    return nullptr;
  const auto &Args = CSE->getTemplateArguments();
  if (Args.size() != 1)
    return nullptr;
  if (Args[0].getKind() != TemplateArgument::Type)
    return nullptr;
  QualType ArgType = Args[0].getAsType();
  const auto *TTPT = ArgType->getAs<TemplateTypeParmType>();
  if (!TTPT)
    return nullptr;
  return TTPT->getDecl() == Param ? CSE : nullptr;
}

void reportOn(ClangTidyCheck &Check, const TemplateParameterList *TPL,
              const Expr *RequiresClause) {
  const auto *Param = singleTypeParam(TPL);
  const auto *CSE = asSingleConceptOnParam(RequiresClause, Param);
  if (!CSE)
    return;
  Check.diag(Param->getLocation(),
             "prefer constraining the template parameter directly "
             "('template <%0 %1>') instead of using a 'requires' clause "
             "for a single concept "
             "(see docs/STYLEGUIDE.md §Concepts in Template Parameter Lists)")
      << CSE->getNamedConcept()->getName() << Param->getName();
}

} // namespace

void ConceptInTemplateParamCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(functionTemplateDecl().bind("ftd"), this);
  Finder->addMatcher(classTemplateDecl().bind("ctd"), this);
}

void ConceptInTemplateParamCheck::check(
    const MatchFinder::MatchResult &Result) {
  if (const auto *FTD = Result.Nodes.getNodeAs<FunctionTemplateDecl>("ftd")) {
    const FunctionDecl *FD = FTD->getTemplatedDecl();
    if (!FD)
      return;
    reportOn(*this, FTD->getTemplateParameters(),
             FD->getTrailingRequiresClause().ConstraintExpr);
    return;
  }
  if (const auto *CTD = Result.Nodes.getNodeAs<ClassTemplateDecl>("ctd")) {
    reportOn(*this, CTD->getTemplateParameters(),
             CTD->getTemplateParameters()->getRequiresClause());
  }
}

} // namespace clang::tidy::gse
