//===--- NoInlineInModulesCheck.cpp - clang-tidy --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NoInlineInModulesCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/StringRef.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {
namespace {

bool isInModuleInterfaceFile(SourceLocation Loc, const SourceManager &SM) {
  if (Loc.isInvalid())
    return false;
  SourceLocation Expansion = SM.getExpansionLoc(Loc);
  if (Expansion.isInvalid())
    return false;
  OptionalFileEntryRef FE = SM.getFileEntryRefForID(SM.getFileID(Expansion));
  if (!FE)
    return false;
  llvm::StringRef Name = FE->getName();
  return Name.ends_with(".cppm") || Name.ends_with(".ixx") ||
         Name.ends_with(".mxx");
}

AST_MATCHER(FunctionDecl, hasInlineSpecifier) {
  return Node.isInlineSpecified();
}

AST_MATCHER(VarDecl, hasInlineSpecifier) {
  return Node.isInlineSpecified();
}

AST_MATCHER(VarDecl, isConstexprVar) {
  return Node.isConstexpr();
}

} // namespace

void NoInlineInModulesCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      functionDecl(hasInlineSpecifier(), unless(isImplicit())).bind("fn"),
      this);
  Finder->addMatcher(
      varDecl(hasInlineSpecifier(), isConstexprVar(),
              hasDeclContext(namespaceDecl()), unless(isImplicit()))
          .bind("v"),
      this);
}

void NoInlineInModulesCheck::check(const MatchFinder::MatchResult &Result) {
  const SourceManager &SM = *Result.SourceManager;
  if (const auto *Fn = Result.Nodes.getNodeAs<FunctionDecl>("fn")) {
    if (!isInModuleInterfaceFile(Fn->getBeginLoc(), SM))
      return;
    diag(Fn->getLocation(),
         "redundant 'inline' on function %0 inside a module interface; "
         "module-attached function definitions have implicit module linkage "
         "(see docs/STYLEGUIDE.md §'inline' on Functions)")
        << Fn;
    return;
  }
  if (const auto *V = Result.Nodes.getNodeAs<VarDecl>("v")) {
    if (!isInModuleInterfaceFile(V->getBeginLoc(), SM))
      return;
    diag(V->getLocation(),
         "redundant 'inline' on constexpr variable %0; namespace-scope "
         "constexpr is implicitly inline since C++17 "
         "(see docs/STYLEGUIDE.md §'inline' on Functions)")
        << V;
  }
}

} // namespace clang::tidy::gse
