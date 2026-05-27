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
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {

namespace {

bool isDeducibleInType(QualType T, const TemplateTypeParmDecl *Target) {
  if (T.isNull())
    return false;
  T = T.getNonReferenceType();
  if (T.isNull())
    return false;
  const Type *TPtr = T.getTypePtr();

  if (const auto *PT = dyn_cast<TemplateTypeParmType>(TPtr))
    return PT->getDecl() == Target;

  if (const auto *PT = dyn_cast<PointerType>(TPtr))
    return isDeducibleInType(PT->getPointeeType(), Target);

  if (const auto *AT = dyn_cast<ArrayType>(TPtr))
    return isDeducibleInType(AT->getElementType(), Target);

  if (const auto *MPT = dyn_cast<MemberPointerType>(TPtr))
    return isDeducibleInType(MPT->getPointeeType(), Target);

  if (const auto *TST = dyn_cast<TemplateSpecializationType>(TPtr)) {
    for (const TemplateArgument &Arg : TST->template_arguments()) {
      if (Arg.getKind() != TemplateArgument::Type)
        continue;
      if (isDeducibleInType(Arg.getAsType(), Target))
        return true;
    }
    return false;
  }

  if (const auto *FPT = dyn_cast<FunctionProtoType>(TPtr)) {
    if (isDeducibleInType(FPT->getReturnType(), Target))
      return true;
    for (QualType P : FPT->getParamTypes()) {
      if (isDeducibleInType(P, Target))
        return true;
    }
    return false;
  }

  if (const auto *PT = dyn_cast<ParenType>(TPtr))
    return isDeducibleInType(PT->getInnerType(), Target);

  return false;
}

bool isParameterDeducible(const FunctionDecl *Pattern,
                          const TemplateTypeParmDecl *Param) {
  for (const ParmVarDecl *P : Pattern->parameters()) {
    if (isDeducibleInType(P->getType(), Param))
      return true;
  }
  return false;
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

  const TemplateParameterList *TPL = Primary->getTemplateParameters();
  if (!TPL)
    return;

  unsigned WrittenCount = Written->NumTemplateArgs;
  if (WrittenCount > TPL->size())
    return;

  for (unsigned I = 0; I < WrittenCount; ++I) {
    const NamedDecl *Param = TPL->getParam(I);
    const auto *TTPD = dyn_cast<TemplateTypeParmDecl>(Param);
    if (!TTPD || TTPD->isParameterPack())
      return;
    if (!isParameterDeducible(Pattern, TTPD))
      return;
  }

  SourceLocation LAngle = Written->getLAngleLoc();
  SourceLocation RAngle = Written->getRAngleLoc();
  if (LAngle.isInvalid() || RAngle.isInvalid())
    return;
  if (Result.SourceManager->isInSystemHeader(LAngle))
    return;

  SourceLocation RemovalEnd =
      Lexer::getLocForEndOfToken(RAngle, 0, *Result.SourceManager,
                                 Result.Context->getLangOpts());

  diag(LAngle,
       "explicit template argument(s) on call to %0 are redundant; "
       "all written arguments are deducible from the function call — "
       "let template argument deduction handle it")
      << FD
      << FixItHint::CreateRemoval(
             CharSourceRange::getCharRange(LAngle, RemovalEnd));
}

} // namespace clang::tidy::gse
