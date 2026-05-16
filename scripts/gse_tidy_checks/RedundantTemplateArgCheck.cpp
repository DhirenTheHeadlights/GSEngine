//===--- RedundantTemplateArgCheck.cpp - clang-tidy -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RedundantTemplateArgCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

namespace {

class CollectTypeParms
    : public RecursiveASTVisitor<CollectTypeParms> {
public:
  llvm::SmallPtrSet<const TemplateTypeParmDecl *, 4> Found;

  bool VisitTemplateTypeParmType(const TemplateTypeParmType *T) {
    if (const auto *D = T->getDecl())
      Found.insert(D);
    return true;
  }
};

llvm::SmallPtrSet<const TemplateTypeParmDecl *, 4>
collectDeducibleTypeParms(const FunctionDecl *FD) {
  CollectTypeParms Collector;
  for (const ParmVarDecl *P : FD->parameters()) {
    QualType T = P->getType();
    if (T.isNull())
      continue;
    Collector.TraverseType(T);
  }
  return std::move(Collector.Found);
}

} // namespace

void RedundantTemplateArgCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(callee(functionDecl(unless(cxxConstructorDecl())).bind("fn")))
          .bind("call"),
      this);
}

void RedundantTemplateArgCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");
  const auto *FD = Result.Nodes.getNodeAs<FunctionDecl>("fn");
  if (!Call || !FD)
    return;

  const FunctionTemplateDecl *Primary = FD->getPrimaryTemplate();
  if (!Primary)
    return;

  const ASTTemplateArgumentListInfo *Written =
      FD->getTemplateSpecializationArgsAsWritten();
  if (!Written || Written->NumTemplateArgs == 0)
    return;

  const FunctionDecl *Pattern = Primary->getTemplatedDecl();
  if (!Pattern)
    return;

  auto Deducible = collectDeducibleTypeParms(Pattern);
  if (Deducible.empty())
    return;

  const TemplateParameterList *TPL = Primary->getTemplateParameters();
  if (!TPL)
    return;

  unsigned WrittenCount = Written->NumTemplateArgs;
  if (WrittenCount > TPL->size())
    return;

  bool AllDeducible = true;
  for (unsigned I = 0; I < WrittenCount; ++I) {
    const NamedDecl *Param = TPL->getParam(I);
    const auto *TTPD = dyn_cast<TemplateTypeParmDecl>(Param);
    if (!TTPD || !Deducible.contains(TTPD)) {
      AllDeducible = false;
      break;
    }
  }
  if (!AllDeducible)
    return;

  SourceRange ArgRange(Written->getLAngleLoc(), Written->getRAngleLoc());
  diag(Written->getLAngleLoc(),
       "explicit template argument(s) on call to %0 are redundant; "
       "all are deducible from the function arguments — let template "
       "deduction handle it")
      << FD
      << FixItHint::CreateRemoval(ArgRange);
}

} // namespace clang::tidy::gse
