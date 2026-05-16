//===--- RedundantTemplateArgCheck.h - clang-tidy ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_GSE_REDUNDANTTEMPLATEARGCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_GSE_REDUNDANTTEMPLATEARGCHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::gse {

class RedundantTemplateArgCheck : public ClangTidyCheck {
public:
  RedundantTemplateArgCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.CPlusPlus;
  }
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace clang::tidy::gse

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_GSE_REDUNDANTTEMPLATEARGCHECK_H
