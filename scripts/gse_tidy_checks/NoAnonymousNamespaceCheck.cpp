//===--- NoAnonymousNamespaceCheck.cpp - clang-tidy -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NoAnonymousNamespaceCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

void NoAnonymousNamespaceCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(namespaceDecl(isAnonymous()).bind("ns"), this);
}

void NoAnonymousNamespaceCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Ns = Result.Nodes.getNodeAs<NamespaceDecl>("ns");
  if (!Ns)
    return;
  diag(Ns->getLocation(),
       "anonymous namespace is forbidden; in modules anything not in an "
       "'export' block is already module-private — use a plain non-exported "
       "namespace block (see docs/STYLEGUIDE.md §Module Visibility)");
}

} // namespace clang::tidy::gse
