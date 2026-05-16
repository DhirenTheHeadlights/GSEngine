//===--- PreferTemplateArgOverExplicitConstructCheck.cpp - clang-tidy -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PreferTemplateArgOverExplicitConstructCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

void PreferTemplateArgOverExplicitConstructCheck::registerMatchers(
    MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(callee(functionDecl().bind("fn"))).bind("call"), this);
}

void PreferTemplateArgOverExplicitConstructCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");
  const auto *FD = Result.Nodes.getNodeAs<FunctionDecl>("fn");
  if (!Call || !FD)
    return;

  const FunctionTemplateDecl *Primary = FD->getPrimaryTemplate();
  if (!Primary)
    return;

  if (FD->getTemplateSpecializationArgsAsWritten() != nullptr)
    return;

  const TemplateArgumentList *Args = FD->getTemplateSpecializationArgs();
  if (!Args)
    return;

  ASTContext &Ctx = *Result.Context;

  for (unsigned I = 0; I < Call->getNumArgs(); ++I) {
    const Expr *Arg = Call->getArg(I)->IgnoreImplicit();
    QualType ArgType;
    SourceLocation ArgLoc;

    if (const auto *Tmp = dyn_cast<CXXTemporaryObjectExpr>(Arg)) {
      ArgType = Tmp->getType();
      ArgLoc = Tmp->getBeginLoc();
    } else if (const auto *Func = dyn_cast<CXXFunctionalCastExpr>(Arg)) {
      ArgType = Func->getType();
      ArgLoc = Func->getBeginLoc();
    } else {
      continue;
    }

    QualType Canonical = Ctx.getCanonicalType(ArgType);

    for (unsigned J = 0; J < Args->size(); ++J) {
      const TemplateArgument &TA = Args->get(J);
      if (TA.getKind() != TemplateArgument::Type)
        continue;
      QualType DeducedCanonical = Ctx.getCanonicalType(TA.getAsType());
      if (DeducedCanonical != Canonical)
        continue;
      diag(ArgLoc,
           "prefer 'foo<%0>({...})' over 'foo(%0{...})' — move the type "
           "into an explicit template argument and use a braced-init "
           "(see docs/STYLEGUIDE.md)")
          << ArgType;
      return;
    }
  }
}

} // namespace clang::tidy::gse
