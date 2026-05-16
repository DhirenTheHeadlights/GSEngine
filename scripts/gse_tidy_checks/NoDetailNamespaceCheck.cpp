//===--- NoDetailNamespaceCheck.cpp - clang-tidy --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NoDetailNamespaceCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

void NoDetailNamespaceCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(namespaceDecl(hasName("detail")).bind("ns"), this);
}

void NoDetailNamespaceCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Ns = Result.Nodes.getNodeAs<NamespaceDecl>("ns");
  if (!Ns || Ns->isAnonymousNamespace())
    return;
  diag(Ns->getLocation(),
       "'detail' namespace is forbidden; in modules, anything not in an "
       "'export' block is already module-private — use a plain non-exported "
       "namespace block (see docs/STYLEGUIDE.md §Module Visibility)");
}

} // namespace clang::tidy::gse
