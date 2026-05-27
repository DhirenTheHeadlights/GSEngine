//===--- RedundantNamespaceQualifierCheck.cpp - clang-tidy ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RedundantNamespaceQualifierCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang::ast_matchers;

namespace clang::tidy::gse {
namespace {

void collectQualifierNamespaces(
    const NestedNameSpecifier *NNS,
    llvm::SmallVectorImpl<const NamespaceDecl *> &Out) {
  for (const NestedNameSpecifier *Cur = NNS; Cur; Cur = Cur->getPrefix()) {
    const NamespaceDecl *NS = Cur->getAsNamespace();
    if (!NS) {
      Out.clear();
      return;
    }
    Out.push_back(NS->getCanonicalDecl());
  }
  std::reverse(Out.begin(), Out.end());
}

void collectEnclosingNamespaces(
    const DeclContext *DC,
    llvm::SmallVectorImpl<const NamespaceDecl *> &Out) {
  for (; DC; DC = DC->getParent()) {
    if (const auto *NS = dyn_cast<NamespaceDecl>(DC))
      Out.push_back(NS->getCanonicalDecl());
  }
  std::reverse(Out.begin(), Out.end());
}

const DeclContext *findEnclosingContext(ASTContext &Ctx,
                                        DynTypedNode Node) {
  while (true) {
    auto Parents = Ctx.getParents(Node);
    if (Parents.empty())
      return nullptr;
    Node = Parents[0];
    if (const auto *D = Node.get<Decl>()) {
      if (const auto *DC = dyn_cast<DeclContext>(D))
        return DC;
    }
  }
}

bool isSameChain(llvm::ArrayRef<const NamespaceDecl *> A,
                 llvm::ArrayRef<const NamespaceDecl *> B) {
  if (A.size() != B.size())
    return false;
  for (size_t I = 0; I < A.size(); ++I) {
    if (A[I] != B[I])
      return false;
  }
  return true;
}

} // namespace

void RedundantNamespaceQualifierCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(nestedNameSpecifierLoc().bind("nns"), this);
}

void RedundantNamespaceQualifierCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *NNSLoc =
      Result.Nodes.getNodeAs<NestedNameSpecifierLoc>("nns");
  if (!NNSLoc)
    return;

  const NestedNameSpecifier *NNS = NNSLoc->getNestedNameSpecifier();
  if (!NNS)
    return;

  llvm::SmallVector<const NamespaceDecl *, 4> QualChain;
  collectQualifierNamespaces(NNS, QualChain);
  if (QualChain.empty())
    return;

  const DeclContext *Enclosing =
      findEnclosingContext(*Result.Context, DynTypedNode::create(*NNSLoc));
  if (!Enclosing)
    return;

  llvm::SmallVector<const NamespaceDecl *, 4> EnclosingChain;
  collectEnclosingNamespaces(Enclosing, EnclosingChain);
  if (EnclosingChain.empty())
    return;

  if (!isSameChain(QualChain, EnclosingChain))
    return;

  SourceLocation Begin = NNSLoc->getBeginLoc();
  SourceLocation End = NNSLoc->getEndLoc();
  if (Begin.isInvalid() || End.isInvalid())
    return;
  if (Result.SourceManager->isInSystemHeader(Begin))
    return;

  CharSourceRange Removal = CharSourceRange::getCharRange(Begin, End);

  diag(Begin,
       "redundant namespace qualifier '%0::' — name is already visible "
       "in the enclosing namespace (see docs/STYLEGUIDE.md §Namespace Qualifiers)")
      << QualChain.back()->getName()
      << FixItHint::CreateRemoval(Removal);
}

} // namespace clang::tidy::gse
