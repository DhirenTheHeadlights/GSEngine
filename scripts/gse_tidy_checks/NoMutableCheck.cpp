//===--- NoMutableCheck.cpp - clang-tidy ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NoMutableCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {
namespace {

AST_MATCHER(FieldDecl, isMutableField) {
  return Node.isMutable();
}

} // namespace

void NoMutableCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(fieldDecl(isMutableField()).bind("f"), this);
}

void NoMutableCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *F = Result.Nodes.getNodeAs<FieldDecl>("f");
  if (!F)
    return;
  diag(F->getLocation(),
       "'mutable' on member %0 is forbidden; use the engine's deferred "
       "mutation mechanism ('defer') to schedule the write at the correct "
       "point in the frame (see docs/STYLEGUIDE.md §Mutation and 'mutable')")
      << F;
}

} // namespace clang::tidy::gse
