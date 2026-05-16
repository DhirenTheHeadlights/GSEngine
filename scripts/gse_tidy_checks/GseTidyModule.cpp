//===--- GseTidyModule.cpp - clang-tidy -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "ConceptInTemplateParamCheck.h"
#include "NoAnonymousNamespaceCheck.h"
#include "NoDetailNamespaceCheck.h"
#include "NoGetPrefixCheck.h"
#include "NoInlineInModulesCheck.h"
#include "NoMutableCheck.h"
#include "PreferTemplateArgOverExplicitConstructCheck.h"
#include "RedundantTemplateArgCheck.h"

namespace clang::tidy {
namespace gse {

class GseModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<ConceptInTemplateParamCheck>(
        "gse-concept-in-template-param");
    CheckFactories.registerCheck<NoAnonymousNamespaceCheck>(
        "gse-no-anonymous-namespace");
    CheckFactories.registerCheck<NoDetailNamespaceCheck>(
        "gse-no-detail-namespace");
    CheckFactories.registerCheck<NoGetPrefixCheck>(
        "gse-no-get-prefix");
    CheckFactories.registerCheck<NoInlineInModulesCheck>(
        "gse-no-inline-in-modules");
    CheckFactories.registerCheck<NoMutableCheck>(
        "gse-no-mutable");
    CheckFactories.registerCheck<PreferTemplateArgOverExplicitConstructCheck>(
        "gse-prefer-template-arg-over-explicit-construct");
    CheckFactories.registerCheck<RedundantTemplateArgCheck>(
        "gse-redundant-template-arg");
  }
};

} // namespace gse

static ClangTidyModuleRegistry::Add<gse::GseModule>
    X("gse-module", "Adds GSE project-specific lint checks.");

volatile int GseModuleAnchorSource = 0;

} // namespace clang::tidy
