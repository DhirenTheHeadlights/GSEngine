set(LLVM_LINK_COMPONENTS
  FrontendOpenMP
  Support
  )

add_clang_library(clangTidyGseModule STATIC
  ConceptInTemplateParamCheck.cpp
  GseTidyModule.cpp
  NoAnonymousNamespaceCheck.cpp
  NoDetailNamespaceCheck.cpp
  NoGetPrefixCheck.cpp
  NoInlineInModulesCheck.cpp
  NoMutableCheck.cpp
  PreferTemplateArgOverExplicitConstructCheck.cpp
  RedundantNamespaceQualifierCheck.cpp
  RedundantTemplateArgCheck.cpp

  LINK_LIBS
  clangTidy
  clangTidyUtils

  DEPENDS
  omp_gen
  ClangDriverOptions
  )

clang_target_link_libraries(clangTidyGseModule
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  clangTooling
  )
