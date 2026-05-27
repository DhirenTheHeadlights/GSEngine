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
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {
namespace {

AST_MATCHER(FieldDecl, isMutableField) {
  return Node.isMutable();
}

CharSourceRange findMutableKeywordRange(SourceLocation Start,
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
        Tok.getRawIdentifier() == "mutable") {
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

void NoMutableCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(fieldDecl(isMutableField()).bind("f"), this);
}

void NoMutableCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *F = Result.Nodes.getNodeAs<FieldDecl>("f");
  if (!F)
    return;

  const SourceManager &SM = *Result.SourceManager;
  const LangOptions &LO = Result.Context->getLangOpts();
  SourceLocation Begin = F->getBeginLoc();
  SourceLocation End = F->getLocation();
  if (Begin.isInvalid() || End.isInvalid() || SM.isInSystemHeader(Begin))
    return;

  auto Diag =
      diag(F->getLocation(),
           "'mutable' on member %0 is forbidden; use the engine's deferred "
           "mutation mechanism ('defer') to schedule the write at the correct "
           "point in the frame "
           "(see docs/STYLEGUIDE.md §Mutation and 'mutable')")
      << F;

  CharSourceRange Range = findMutableKeywordRange(Begin, End, SM, LO);
  if (Range.isValid())
    Diag << FixItHint::CreateRemoval(Range);
}

} // namespace clang::tidy::gse
