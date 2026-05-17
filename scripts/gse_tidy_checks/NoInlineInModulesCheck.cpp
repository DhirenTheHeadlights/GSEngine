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
#include "clang/Basic/Module.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringRef.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {
namespace {

AST_POLYMORPHIC_MATCHER(hasInlineSpecifier,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(FunctionDecl,
                                                        VarDecl)) {
  return Node.isInlineSpecified();
}

AST_MATCHER(VarDecl, isConstexprVar) {
  return Node.isConstexpr();
}

CharSourceRange findInlineKeywordRange(SourceLocation Start,
                                       SourceLocation Stop,
                                       const SourceManager &SM,
                                       const LangOptions &LO) {
  SourceLocation Cur = Start;
  while (Cur.isValid() &&
         SM.isBeforeInTranslationUnit(Cur, Stop)) {
    Token Tok;
    if (Lexer::getRawToken(Cur, Tok, SM, LO, /*IgnoreWhiteSpace=*/true))
      break;
    if (Tok.is(tok::raw_identifier) &&
        Tok.getRawIdentifier() == "inline") {
      SourceLocation TokStart = Tok.getLocation();
      SourceLocation TokEnd =
          Lexer::getLocForEndOfToken(TokStart, 0, SM, LO);
      const char *AfterPtr = SM.getCharacterData(TokEnd);
      unsigned Skip = 0;
      while (AfterPtr[Skip] == ' ' || AfterPtr[Skip] == '\t')
        ++Skip;
      return CharSourceRange::getCharRange(
          TokStart, TokEnd.getLocWithOffset(Skip));
    }
    SourceLocation Next =
        Lexer::getLocForEndOfToken(Tok.getLocation(), 0, SM, LO);
    if (!Next.isValid() || Next == Cur)
      break;
    Cur = Next;
  }
  return {};
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
  if (!Result.Context->getCurrentNamedModule())
    return;

  const SourceManager &SM = *Result.SourceManager;
  const LangOptions &LO = Result.Context->getLangOpts();

  auto emit = [&](const NamedDecl *D, llvm::StringRef Kind,
                  llvm::StringRef Reason) {
    SourceLocation Begin = D->getBeginLoc();
    SourceLocation End = D->getLocation();
    if (Begin.isInvalid() || End.isInvalid())
      return;
    if (SM.isInSystemHeader(Begin))
      return;

    auto Diag = diag(D->getLocation(),
                     "redundant 'inline' on %0 %1; %2 "
                     "(see docs/STYLEGUIDE.md §'inline' on Functions)")
                << Kind << D << Reason;

    CharSourceRange InlineRange = findInlineKeywordRange(Begin, End, SM, LO);
    if (InlineRange.isValid())
      Diag << FixItHint::CreateRemoval(InlineRange);
  };

  if (const auto *Fn = Result.Nodes.getNodeAs<FunctionDecl>("fn")) {
    emit(Fn, "function",
         "module-attached function definitions already have implicit module "
         "linkage");
    return;
  }
  if (const auto *V = Result.Nodes.getNodeAs<VarDecl>("v")) {
    emit(V, "constexpr variable",
         "namespace-scope constexpr is implicitly inline since C++17");
  }
}

} // namespace clang::tidy::gse
