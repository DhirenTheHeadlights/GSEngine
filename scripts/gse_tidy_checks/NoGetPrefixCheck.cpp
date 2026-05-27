//===--- NoGetPrefixCheck.cpp - clang-tidy --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NoGetPrefixCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

void NoGetPrefixCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      functionDecl(matchesName("::get_[^_]"),
                   unless(anyOf(isImplicit(),
                                cxxMethodDecl(isOverride()))))
          .bind("fn"),
      this);
}

void NoGetPrefixCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Fn = Result.Nodes.getNodeAs<FunctionDecl>("fn");
  if (!Fn || !Fn->getIdentifier())
    return;
  StringRef Name = Fn->getName();
  if (!Name.starts_with("get_") || Name.size() <= 4)
    return;
  StringRef Stripped = Name.drop_front(4);
  if (Stripped.empty() || (Stripped.front() >= '0' && Stripped.front() <= '9'))
    return;

  SourceLocation Begin = Fn->getLocation();
  SourceLocation End = Begin.getLocWithOffset(static_cast<int>(Name.size()) - 1);
  diag(Begin,
       "function name %0 uses banned 'get_' prefix; drop it "
       "(see docs/STYLEGUIDE.md §Naming)")
      << Fn
      << FixItHint::CreateReplacement(SourceRange(Begin, End), Stripped);
}

} // namespace clang::tidy::gse
