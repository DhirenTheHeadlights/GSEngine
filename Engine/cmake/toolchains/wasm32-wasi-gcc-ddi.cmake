# The wasm32 driver appends its object suffix to -fdeps-target, so every scan names
# <OBJECT>.o as the primary output and CMake's collation finds no FILE_SET source that
# produces it. Run after each scan with -DDDI=<the scan's ddi>, this puts back the object
# path the rule asked for.

file(READ "${DDI}" _gse_ddi)
string(REGEX REPLACE "(\"primary-output\": \"[^\"]*\\.o)\\.o\"" "\\1\"" _gse_fixed "${_gse_ddi}")
if(NOT _gse_fixed STREQUAL _gse_ddi)
    file(WRITE "${DDI}" "${_gse_fixed}")
endif()
