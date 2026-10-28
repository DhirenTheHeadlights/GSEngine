"""Build an experimental wasm32 GCC cross compiler from a trunk snapshot plus the
vendored WebAssembly backend patches (Tools/gcc-toolchain/wasm).

This is NOT the toolchain the engine is built with, and it does not touch it. The
backend is an out-of-tree patch series (see Tools/gcc-toolchain/wasm/README.md), so
this builds into its own prefix: driver, cc1, cc1plus, libgcc and libstdc++, against
a downloaded wasi-libc sysroot. Measuring what that compiler can and cannot do is the
point — see docs/wasm_pipeline_plan.md.

It used to stop at `make all-gcc`, with no target library at all, because there was no
libc for one to be built against and no linker to put the result together. Both of
those are gone: step 7 closed linking, and wasi-libc supplies the libc.

Probes that carry an expected result are assembled and executed, not just compiled:
llvm-mc turns the emitted assembly into a relocatable object, wasm-ld links it, and
node runs it. That is the only way to tell correct codegen from plausible-looking
codegen, and it is what every later claim about this backend gets measured against.

There is one run pipeline where there used to be two. The old `unlinked` one was
wat2wasm on the emitted module text, and it was worth having because it answered "is
this codegen right" independently of "can this output be linked" — rwat could place
neither a static nor a function pointer, so the linked pipeline was red for reasons
that had nothing to do with GCC. It cannot survive the move to llvm-mc, whose input is
assembly rather than a module, and it no longer has a question to answer: see
docs/wasm_pipeline_plan.md "Step 7". The cost is that a miscompile now surfaces only
through the link.

Unlike build_gcc_trunk.py this configures GCC directly instead of going through
makepkg. The vendored PKGBUILD exists to make GCC build *for* native Windows, and its
patch stack is mingw-target-specific; a cross whose host compiler is the already-working
MSYS2 gcc needs none of it.

    python Tools/build_gcc_wasm.py --snapshot 20260809
"""
import argparse
import hashlib
import os
import re
import shutil
from pathlib import Path
from typing import NamedTuple, Optional

import module_graph
from msys2 import DEFAULT_MSYS_ROOT, REPO_ROOT, bash, to_msys
from repo_paths import GENERATED_ROOT
# The harness and the exit taxonomy every run probe is read with. Imported from the
# scenario runner rather than restated, so a probe's `ran` and a scenario's exit 0 are
# the same verdict.
from run_wasm import (ENTRY, HARNESS_MAX_PAGES, NO_INSTANCE_EXIT, PAGE_SIZE, TRAP_EXIT,
                      write_harness)

# The snapshot the engine's own native toolchain is built from, which is what this has
# to be for an engine-source probe to mean anything: the series' merge base, 20260524,
# rejects `Hash.cppm' and fourteen modules cascade off it, and that is eleven weeks of
# frontend drift rather than a target gap (step 14). Matching the engine's compiler is
# also what keeps a red row readable -- a failure here is then the target's, not a
# difference between two trunks.
#
# Not the newest snapshot, deliberately. Newest would add nine more weeks of middle-end
# drift for the patch series to survive, and would make every red engine row ambiguous
# between a target gap and a trunk regression the engine has never seen.
DEFAULT_SNAPSHOT = "20260809"

# config.sub rejects LLVM's wasm32-unknown-unknown ("OS `unknown' not recognized") and
# canonicalises wasm32-none to this. wasm32-wasi also configures, but names a WASI libc
# that does not exist here; the backend's config.gcc stanza matches any wasm*-*-*.
TARGET = "wasm32-unknown-none"

SNAPSHOT_URL = "https://gcc.gnu.org/pub/gcc/snapshots/17-{snapshot}/gcc-17-{snapshot}.tar.xz"

# wasi-libc, as the target's C library. It is the only libc that exists for wasm32
# without writing one, and taking it is what makes a *hosted* libstdc++ possible --
# which is the whole point, because libstdc++'s `std` module is built only in hosted
# mode (libstdc++-v3/src/c++23/Makefile.am gates std.cc on GLIBCXX_HOSTED), and the
# engine's own code imports std.
#
# Its objects are ordinary wasm objects, so wasm-ld links them beside GCC's without
# anything knowing which compiler produced which -- a property the move to llvm-mc
# bought and the annotated dialect could never have had.
#
# Pinned, and the pin is load-bearing: wasi-libc's headers are only as C++-clean as
# the clang they were last tested against. 27.0's wasi/api.h has bare
# `_Static_assert (_Alignof (...))', which no C++ compiler accepts and which the first
# #include <cstdio> reaches; 32.0 does not. What no checked release avoids is
# `_Thread_local' on errno -- see WASI_TARGET.
#
# The cost, stated because it is load-bearing later: this makes the measurement target
# WASI, not a browser. A browser build needs either a WASI shim in JS or the engine's
# own host layer in place of one. That is a known-shaped job and the engine needs a
# host layer regardless, but nothing here has measured it.
WASI_SYSROOT_VERSION = "32.0"
WASI_SYSROOT_URL = ("https://github.com/WebAssembly/wasi-sdk/releases/download/"
                    "wasi-sdk-{major}/wasi-sysroot-{version}.tar.gz")

# Which of the tarball's targets is flattened. This decides what the measurement is OF
# and not merely where the files come from, so it belongs in the sysroot's cache key
# beside the version -- and is in it, for the reason stated on SYSROOT_EDITS below.
#
# wasm32-wasip1-threads is the threads variant, taken in step 19. It is what makes
# <mutex> declare anything: libstdc++ chooses its locking and atomicity implementation
# from whether a gthread model is present, and the non-threads variant has no pthreads
# for --enable-threads=posix to find. That is why six of step 18's eight rejected units
# were std::mutex not existing.
#
# It is not simply a longer file list. The variant declares errno thread-local for real,
# which is what makes native TLS in the backend a prerequisite rather than a nicety --
# see SYSROOT_EDITS.
WASI_TARGET = "wasm32-wasip1-threads"

# What declare_setjmp does to the flattened headers, named so the sysroot stamp can
# record it. The sysroot's cache key is every input that decides what was measured, and
# an edit to a header is one of those as much as the version is: without this in the
# key, an existing workdir goes on measuring the unedited headers and reports that as
# the result of the edit.
#
# `errno-plain' was the second entry and is gone, with plain_errno itself. Step 17 added
# it because a non-threads wasi-libc built by clang puts errno in ordinary data while
# its header claims `_Thread_local', so GCC asked for an emutls control object nobody
# defined. On the threads variant the header is telling the truth -- errno really is a
# TLS symbol in libc.a -- so rewriting the declaration would be step 17's bug pointed
# the other way: C++ would read a non-thread-local address that libc never writes, and
# every thread but the first would see another thread's errno. What replaces the rewrite
# is the backend actually implementing TLS.
SYSROOT_EDITS = "setjmp-declared"

# Tools the run stage needs, installed into the repo-local MSYS2 on demand. Deliberately
# not added to install_msys2.py's list: that list is read out of build-gcc-trunk.yml so
# CI and local builds share one package closure, and these are not GCC build dependencies.
# llvm supplies llvm-mc, the assembler the backend's ASM_SPEC now names; lld supplies
# wasm-ld; node runs the result. llvm-mc and wasm-ld have to come from one LLVM release,
# which they do here by both being MSYS2 packages: a relocation the assembler emits is
# only useful if the linker that consumes it understands the same numbering. wabt is
# here for wasm-objdump, which no probe runs but every investigation of one does, and
# which is how wasm_dialect_probe.py asserts its subjects' object facts.
RUN_TOOLS = ("mingw-w64-x86_64-wabt", "mingw-w64-x86_64-nodejs",
             "mingw-w64-x86_64-lld", "mingw-w64-x86_64-llvm")

# The triple handed to llvm-mc, which is not the one GCC is configured for: LLVM calls
# the OS-less wasm target `unknown', GCC's config.sub canonicalises it to `none'. The
# two name the same target; only the spelling differs, and ASM_SPEC carries this one.
ASM_TRIPLE = "wasm32-unknown-unknown"

# The instruction-set features the assembler has to be told to accept, and the reason
# this is a constant rather than a literal at the one call site: it has to match the
# `-mattr' in the backend's own ASM_SPEC, because the driver assembles libgcc and
# libstdc++ through that spec while this script assembles every probe by hand. Two
# spellings of the same list is a probe table measuring a different assembler from the
# one the engine's objects are built with -- the same measurement bug as the
# ENGINE_CXXFLAGS one, which step 18 paid for in ten misattributed red rows.
#
# `check_asm_features' enforces the match against wasm.h rather than trusting this
# comment, because a constant that merely claims to agree with a patch is how the
# first version of this got it wrong: the probes assembled with no -mattr at all and
# passed, since no probe's own translation unit emitted an atomic. The ones that
# would have failed are the next feature-gated family's.
#
# Both features, not one: see 0015. wasm-ld refuses thread-local storage unless the
# object declares atomics AND bulk-memory, so they are enabled as a pair.
ASM_FEATURES = "+atomics,+bulk-memory"

CXXFLAGS = "-std=c++26 -fno-exceptions -fno-rtti"

# What wasi-libc demands of any C++ translation unit that reaches far enough into it, and
# so of every probe that uses the C++ runtime. It is already in CXXFLAGS_FOR_TARGET for
# the library build; this is the same statement for the compiles that happen outside it:
# wasi-libc's signal.h is an #error without it, reached by <csignal>, and the matching
# -lwasi-emulated-signal is already on the hosted link line.
#
# `-D_Thread_local=__thread' is here beside it again, and on the threads variant it is
# load-bearing rather than the inert leftover step 17 left it as. `_Thread_local' is C11
# and not a C++ keyword, so without the define the `extern _Thread_local int errno' that
# <cerrno> reaches does not parse as thread-local at all -- and on this variant it has
# to, because libc.a really does define errno as a TLS symbol. Step 17's objection to
# the define was to mapping it onto a target with no TLS; the target has TLS now.
CXX_RUNTIME_DEFINES = "-D_WASI_EMULATED_SIGNAL -D_Thread_local=__thread"


# COMPILE stops at the compiler, which is all the frontend questions need. LINKED goes
# all the way to a number out of node.
COMPILE, LINKED = "compile", "linked"


class Blocked(NamedTuple):
    """A known-red (probe, mode), and the exact status it is known to be red with.

    The status is as load-bearing as the reason: a blocked probe that fails some *other*
    way is a new regression wearing a known gap's clothes, and excusing it would turn the
    probe from a tripwire into a blindfold. A blocked probe that passes is reported louder
    still.
    """
    mode: str
    status: str
    reason: str


class Probe(NamedTuple):
    """A C++ translation unit and how far it is expected to get.

    `expect` is the integer `entry` must return once the module runs; probes that leave
    it None are compile-only. `second` is a second translation unit linked with the
    first, for the one question a single TU cannot ask.

    `sources` names real repo files, relative to the repo root, compiled in the order
    given and ahead of `body`. It is how the engine's own code becomes a probe: a
    module interface unit has to be compiled before anything that imports it, and that
    order is a fact about the engine rather than about this script, so it is written
    out here rather than derived. A probe may have `sources` and a `body` both -- the
    sources bring a module up and the body is the translation unit that uses it.

    `hosted` says the probe needs the target's C runtime rather than only the
    compiler: the sysroot's headers on the way in, and wasi-libc on the link line.
    `cxx_runtime` adds libstdc++ and libsupc++ to that. They are separate, and off by
    default, for the reason the probe table exists at all -- a probe that does not need
    a libc should not be able to fail because of one, and a probe that needs only a
    libc should not be able to fail because libstdc++ is missing.

    `std_module` says the `std` module has to be compiled before this probe can import
    it. GCC builds no module implicitly and libstdc++ installs `std.cc` as a *source*
    file, so `import std;` without this is an error about a missing `.gcm` rather than
    anything about the target.
    """
    name: str
    flags: str
    body: str
    expect: Optional[int] = None
    blocked: tuple = ()
    second: str = ""
    sources: tuple = ()
    # A module whose derived closure, or whose partitions, are this probe's sources.
    # Resolved when the probe runs rather than when this table is built, because the
    # graph is read out of a configured native build directory and nothing else in this
    # script needs one -- see `graph`.
    closure_of: str = ""
    partitions_of: str = ""
    hosted: bool = False
    cxx_runtime: bool = False
    std_module: bool = False
    # `threads` says the probe needs the target's threads: the pthread archive on the
    # link line, a shared linear memory, and a harness that services wasi.thread-spawn.
    # It is one field rather than three because the three are not independently useful
    # -- wasm-ld refuses thread-local storage unless the atomics and bulk-memory
    # features are present, and refuses a shared memory without a maximum, so a probe
    # asking for any part of this needs all of it. Measured in step 19a, where a
    # hand-written subject with TLS and no features was rejected outright.
    threads: bool = False


# The engine's own modules, derived. These used to be four hand-written lists, with a
# comment arguing that the order "is a fact about the engine rather than about this
# script, so it is written out here rather than derived" -- true, and the reason they
# went: `module_graph.py` derives the same fact from CMake's compile database and
# dependency scan. The lists were kept as the oracle for exactly one run: all three
# closures below came out equal as sets to what was recorded, and the derived order
# topologically valid, before they were deleted.
#
# `gse.meta` is self-contained on `std` and `gse.assert`'s interface is too, so the
# closures nest: gse.math's 36 units are gse.meta's 16 plus gse.assert's 1 plus its own
# 19. That is not asserted here -- it is what the graph says, and a change to the
# engine's imports is allowed to change it.
#
# Lazily, and that matters: the graph is read out of the configured wasm32 build
# directory, which a clean checkout does not have. Scanning at import time made that
# directory a prerequisite of the whole script -- `--help` and all seventeen steps of
# building the cross compiler, none of which have anything to do with the engine's
# modules. Only the measurement needs it, so only the measurement asks for it.
_GRAPH = None


def graph() -> module_graph.Graph:
    """The engine's module graph, scanned once per run and only if something asks."""
    global _GRAPH
    if _GRAPH is None:
        _GRAPH = module_graph.scan()
    return _GRAPH

# What the engine is built with, minus the two things this target cannot honour:
# -mavx2, which is why `SIMD.cppm` now has a portable arm, and exceptions, which are
# off because the std module and libstdc++ here were built that way and because a
# throw aborts on this target regardless.
#
# The two -W options are `Engine/cmake/GSEEngine.cmake`'s, and leaving them out was a
# measurement bug the first sweep found: `-Wchanges-meaning` is an error by default, the
# engine turns it off in both spellings, and the engine's own code relies on that -- a
# member named for the type it holds (`contact_point::separation`, `request::target`,
# `buffer::handle()`) is the shape it fires on. Without them ten units of gse.graphics,
# gse.physics, gse.gpu_backend and gse.runtime failed here while compiling natively, which
# is this script's own include-path hazard wearing different clothes: a red that looks
# like the target and is really a flag the engine passes and the harness did not. Any flag
# the engine relies on belongs here or an engine probe measures a compiler the engine
# never uses.
ENGINE_CXXFLAGS = (CXXFLAGS + " -fmodules -freflection -fcontracts"
                   " -Wno-changes-meaning -Wno-error=changes-meaning")

# The engine's own include directory, `Engine/CMakeLists.txt`'s one
# target_include_directories entry, and the same hazard the paragraph above records. No
# compile here needed it until now: a wrapper under Source/External was the only kind of
# unit that #includes a vendored header, and every one of them was short-circuited into a
# host-bound row before the compiler saw it. `gse.moodycamel` is the first that is
# compiled, and without this flag its row would read FAILED on a header the engine finds
# -- this harness's include-path hazard again, wearing the target's clothes.
#
# Handed out by `target_cxxflags` and not appended beside ENGINE_CXXFLAGS at a call site,
# which is the whole point: every probe reads its include flags from that one function,
# and an engine probe whose closure reaches moodycamel -- `closure_of="gse.time"` is the
# obvious next one -- has to be compiled with what the engine's own build uses or the two
# measure different compilers.
ENGINE_INCLUDE_DIR = REPO_ROOT / "Engine" / "External"

PROBES = (
    Probe("baseline", CXXFLAGS, """
struct point { int x, y; };
constexpr int add(int a, int b) { return a + b; }
static point p{add(1, 2), 4};
int sum() { return p.x + p.y; }
"""),
    # Core-language reflection only: there is no <meta> to include on a freestanding target.
    Probe("reflection", CXXFLAGS + " -freflection", """
struct point { int x, y; };
constexpr auto r = ^^point;
using alias = typename [:r:];
constexpr int sum() { alias p{1, 2}; return p.x + p.y; }
static_assert (sum () == 3);
int entry() { return sum (); }
"""),
    Probe("contracts", CXXFLAGS + " -fcontracts", """
int halve(int n) pre (n % 2 == 0) { return n / 2; }
int call() { return halve(4); }
"""),
    # Arithmetic through a loop and a branch: the one probe that proves the br_table
    # dispatch loop the backend emits for every CFG actually computes the right answer.
    Probe("arith", CXXFLAGS, """
extern "C" int entry ()
{
  int acc = 0;
  for (int i = 1; i <= 10; ++i)
    acc += (i % 3 == 0) ? i * 2 : i;
  return acc;
}
""", expect=73),
    # Varargs: nvptx's recipe, an implicit pointer to the outgoing-args block in linear
    # memory. Nothing here needs a relocation, so it runs without a linker.
    Probe("varargs", CXXFLAGS, """
#include <stdarg.h>

extern "C" int total (int count, ...)
{
  va_list ap;
  va_start (ap, count);
  int acc = 0;
  for (int i = 0; i < count; ++i)
    acc += va_arg (ap, int);
  va_end (ap);
  return acc;
}

extern "C" int entry () { return total (4, 10, 20, 30, 40); }
""", expect=100),
    # An internal-linkage callee, which is what almost all real code looks like and what
    # nothing else here covers: baseline, contracts and reflection carry no `expect`, so
    # they are never taken through the linked pipeline. It is also the second probe of a
    # cross-function direct call, so a lost `(@reloc)` on `call` shows up here too.
    Probe("local_linkage", CXXFLAGS, """
static int scale (int n) { return n * 3; }

extern "C" int entry () { return scale (14) + 7; }
""", expect=49),
    # Two call targets, so a function pointer that resolved to the wrong index cannot
    # pass by landing on the right function anyway. This is one of the two rows the move
    # to llvm-mc exists to turn green: a function address in data is a TABLE_INDEX
    # relocation, which the annotated dialect had no way to ask for.
    Probe("funcptr", CXXFLAGS, """
extern "C" int twice (int n) { return n * 2; }
extern "C" int thrice (int n) { return n * 3; }

extern "C" int entry ()
{
  int (*fns[2]) (int) = { twice, thrice };
  int acc = 0;
  for (int i = 0; i < 2; ++i)
    acc += fns[i] (7);
  return acc;
}
""", expect=35),
    # Two statics, the other row the dialect move exists to turn green: each is a data
    # symbol in its own section with a MEMORY_ADDR relocation naming it, where the
    # annotated dialect emitted both at (i32.const 0) and had no data symbol to place
    # them by, so they overlapped and `entry` returned 2002.
    Probe("static", CXXFLAGS, """
int lo = 40;
int hi = 2;

extern "C" int entry () { return lo + hi * 1000; }
""", expect=2040),
    # A varargs callee this unit only declares, which is the case where a signature
    # derived from the type and a call site derived from the registers can disagree
    # without anything noticing: the hidden trailing pointer to the outgoing-args
    # block is a parameter the argument list does not mention. `varargs` cannot catch
    # it, because there the definition is in the same TU and its signature wins.
    Probe("extern_varargs", CXXFLAGS, """
extern "C" int sum_all (int count, ...);

extern "C" int entry () { return sum_all (3, 11, 22, 33); }
""", expect=66, second="""
#include <stdarg.h>

extern "C" int sum_all (int count, ...)
{
  va_list ap;
  va_start (ap, count);
  int acc = 0;
  for (int i = 0; i < count; ++i)
    acc += va_arg (ap, int);
  va_end (ap);
  return acc;
}
"""),
    # A result too large to come back in the return local, which is the other half of
    # the same question: the caller passes a hidden leading pointer and the callee
    # writes through it, so the function declares no return local and must not emit a
    # `return` that reads one. Nothing else here returns anything but an int.
    Probe("sret", CXXFLAGS, """
struct big { int a[8]; };

extern "C" big make (int seed)
{
  big out;
  for (int i = 0; i < 8; ++i)
    out.a[i] = seed + i;
  return out;
}

extern "C" int entry ()
{
  big b = make (5);
  int acc = 0;
  for (int i = 0; i < 8; ++i)
    acc += b.a[i];
  return acc;
}
""", expect=68),
    # A static constructor, which nothing measured until now and which upstream marked
    # with an annotation no assembler read, so constructors silently did not run. The
    # initialiser has to be something the frontend cannot fold into the object's
    # initial bytes, or the probe passes whether or not a constructor ever ran — hence
    # the volatile source for the value.
    #
    # Hosted libstdc++ is what makes this stop being a footnote: ios_base::Init and the
    # locale tables are static constructors, so a `std` module without this does not
    # work and does not say so.
    Probe("init_array", CXXFLAGS, """
static volatile int seed = 7;

struct counter
{
  int value;
  counter () : value (seed * 6) {}
};

static counter c;

extern "C" int entry () { return c.value; }
""", expect=42),
    # A libcall: a call the compiler writes and the probe does not, resolved out of
    # libgcc rather than out of either translation unit. It is the only probe that can
    # say whether libgcc was built at all, and the only one that would notice it
    # disappearing again.
    #
    # Finding one took measuring rather than guessing, because wasm has far more
    # instructions than the usual soft-float target: i64 divide, modulo, shifts and
    # every float conversion are single opcodes, `__int128` is rejected outright on a
    # 32-bit target, and `long double` is `double` here, so none of the shapes that
    # would call libgcc anywhere else do so on wasm. `__builtin_powi` does — it is
    # `__powidf2`, which no ISA has.
    #
    # It therefore also measures something no other probe can: a libgcc signature is
    # derived from a GCC-internal builtin declaration rather than from a C++ one the
    # probe wrote, and the emitted `.functype` is `(i32, f64) -> (f64)` while libgcc
    # defines `__powidf2 (double, int)`. If that ordering is the emitter's and not just
    # a spelling, this probe returns the wrong number.
    Probe("libcall", CXXFLAGS, """
static volatile double base = 3.0;

extern "C" int entry () { return (int) __builtin_powi (base, 5); }
""", expect=243),
    # __multi3, the one symbol that stood between a clang-built sysroot and any
    # hosted C++ link: wasi-libc's intscan.c calls it, and libstdc++'s locale setup
    # reaches intscan. This compiler never emits a call to it -- TImode is vetoed, so
    # there is no __int128 to multiply -- so the routine exists purely so that objects
    # built by the compiler the rest of the ecosystem uses can be linked against ours.
    #
    # It computes rather than links, and that is the point of the row. A symbol libgcc
    # does not define is a link error anyone notices; a hand-written 64x64 -> 128
    # multiply that drops the carry out of its two middle partial products links
    # perfectly and is wrong only on the inputs nobody picks. So the operands are
    # volatile and chosen to carry through every stage, and both result words are
    # checked: 100 for the low word, 23 for the high, 123 for a routine that is right.
    #
    # The ABI this declares is clang's, read off intscan.c.obj's type section rather
    # than assumed -- (i32, i64, i64, i64, i64) -> nil, a result pointer first and each
    # 128-bit argument as two i64s low word first. This probe cannot check that reading,
    # since it shares it; `include_vector` linking without wasm-ld's `function signature
    # mismatch' is what checks it, and that is the row to watch if this one stays green
    # while the hosted ones do not.
    Probe("multi3", CXXFLAGS, """
extern "C" void __multi3 (unsigned long long *result,
                          unsigned long long a_lo, unsigned long long a_hi,
                          unsigned long long b_lo, unsigned long long b_hi);

static volatile unsigned long long a_lo = 0x1234567890ABCDEFull;
static volatile unsigned long long a_hi = 0x0F0F0F0F0F0F0F0Full;
static volatile unsigned long long b_lo = 0xFEDCBA9876543210ull;
static volatile unsigned long long b_hi = 0x00000000DEADBEEFull;

extern "C" int entry ()
{
  unsigned long long product[2] = {0, 0};
  __multi3 (product, a_lo, a_hi, b_lo, b_hi);

  return (product[0] == 0x236D88FE55618CF0ull ? 100 : 0)
         + (product[1] == 0x5EA231548A055A53ull ? 23 : 0);
}
""", expect=123),
    # Two translation units each defining an internal `scale`, which is the hazard the
    # annotated dialect carried and nothing measured: rwat emitted every symbol global
    # and strong whatever its linkage, so these two collided. Local binding is now a
    # `.globl` withheld, and this is the only probe that can tell whether it was.
    Probe("two_tu", CXXFLAGS, """
extern "C" int part_b ();

static int scale (int n) { return n * 3; }

extern "C" int entry () { return scale (14) + 7 + part_b (); }
""", expect=249, second="""
static int scale (int n) { return n * 100; }

extern "C" int part_b () { return scale (2); }
"""),
    # Two translation units each defining the same one-only decls, which is what a
    # header looks like in real code and what the target had no way to express: without
    # MAKE_DECL_ONE_ONLY an inline function and a vtable are deduplicated only by the
    # plain-weak fallback, with no group tying a vtable to the rest of its class.
    #
    # It is a tripwire rather than a measurement of the dedup itself, and the comment
    # says so because the two are easy to confuse: the linker resolves two weak
    # definitions to one either way, so no return value can tell the group from the
    # fallback. What it does catch is both ways the group emission goes wrong, and both
    # were hit by hand getting here -- a malformed group directive, which llvm-mc
    # rejects, and a following function swallowed into the group because plain `.text'
    # does not end one, which surfaces as `undefined symbol' for a symbol the object
    # plainly defines. The structural side is asserted in Tools/wasm_dialect_probe.py,
    # where a subject can name what must appear in the object.
    #
    # -fno-rtti is in CXXFLAGS, so the vtable here needs no typeinfo and the probe stays
    # freestanding; a virtual destructor is deliberately absent, since the deleting
    # variant would reach for an operator delete this probe has no libc for.
    Probe("comdat_tu", CXXFLAGS, """
struct shape
{
  virtual int sides () const { return 3; }
};

inline int doubled (int n) { return n * 2; }

extern "C" int part_b ();

extern "C" int entry ()
{
  shape s;
  return doubled (s.sides ()) + part_b ();
}
""", expect=606, second="""
struct shape
{
  virtual int sides () const { return 3; }
};

inline int doubled (int n) { return n * 2; }

extern "C" int part_b ()
{
  shape s;
  return doubled (s.sides ()) * 100;
}
"""),
    # The first probe to need a target libc at all, and the first to run under a real
    # WASI host rather than against stub imports: malloc, a string function and stdio,
    # which between them are most of what libstdc++'s own configure tests link.
    Probe("libc", CXXFLAGS, """
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" int entry ()
{
  char *s = (char *) malloc (32);
  strcpy (s, "libc");
  printf ("probe sees %s\\n", s);
  int n = (int) strlen (s);
  free (s);
  return n * 10;
}
""", expect=40, hosted=True),
    # A container, which is the shortest statement of "hosted libstdc++ works": it
    # needs the headers, the library, the operator new in libsupc++ and the allocator
    # underneath that, so a gap in any of those four shows up here.
    Probe("include_vector", CXXFLAGS, """
#include <vector>

extern "C" int entry ()
{
  std::vector<int> v;
  for (int i = 1; i <= 10; ++i)
    v.push_back (i * i);
  int acc = 0;
  for (int n : v)
    acc += n;
  return acc;
}
""", expect=385, hosted=True, cxx_runtime=True),
    # errno, which has to be the *same* integer on both sides of the language boundary.
    # It computes rather than links on purpose: a missing __emutls_v.errno is a link
    # error anyone notices, while an errno that is private to the C++ side links
    # perfectly and then reports success for every failing libc call. So the probe
    # never writes the value it reads -- it clears errno, hands an out-of-range literal
    # to libc's own integer parser, and asks whether what libc set is what C++ sees.
    #
    # It was step 17's row for the sysroot edit that is now gone, and it keeps its job
    # exactly: on the threads variant errno is a real TLS symbol, so this is the probe
    # that says GCC's TLS addressing reaches the same storage libc writes. Red here
    # means the backend's TLS is wrong, and it would otherwise be invisible -- a
    # per-thread copy C++ alone can see links perfectly and reports success for every
    # failing libc call.
    Probe("errno_shared", CXXFLAGS, """
#include <cerrno>
#include <climits>
#include <cstdlib>

extern "C" int entry ()
{
  errno = 0;
  const long parsed = std::strtol ("99999999999999999999", nullptr, 10);
  return (errno == ERANGE && parsed == LONG_MAX) ? 7 : 0;
}
""", expect=7, hosted=True, cxx_runtime=True),
    # Step 19's three, in the order they narrow a failure: the compiler's own TLS, then
    # libstdc++ declaring a mutex at all, then a thread actually running.
    #
    # A thread_local across two translation units, which is the question one TU cannot
    # ask. `0015' emits a .tdata definition in one object and a `@TLSREL' reference in
    # the other, and only the link decides whether those agree -- a single TU could be
    # self-consistent and wrong. It is also the row that would catch the TLS block being
    # laid out per-object instead of once.
    Probe("thread_local_shared", CXXFLAGS, """
extern int bump ();

extern "C" int entry ()
{
  return bump () + bump () + bump ();
}
""", expect=42, second="""
thread_local int counter = 12;

int bump ()
{
  counter += 1;
  return counter;
}
"""),
    # std::mutex existing, which is what six of step 18's eight rejected engine units
    # needed and the reason this step was taken. It locks and unlocks rather than only
    # declaring one, so the row also covers pthread_mutex_lock being reachable at the
    # link -- `<mutex>` compiling and the libc call resolving are different facts.
    Probe("std_mutex", CXXFLAGS, """
#include <mutex>

static std::mutex m;
static int guarded = 7;

extern "C" int entry ()
{
  {
    std::lock_guard<std::mutex> lock (m);
    guarded += 35;
  }
  std::unique_lock<std::mutex> second (m);
  return second.owns_lock () ? guarded : 0;
}
""", expect=42, hosted=True, cxx_runtime=True, threads=True),
    # A thread that actually runs, joined, with its result read back. The point of
    # taking real pthreads rather than a single-threaded gthread model was that this
    # row is reachable at all, so it is the row that says the trade paid off: a
    # declarations-only model would compile the two above and never this one.
    Probe("std_thread", CXXFLAGS, """
#include <thread>

static int produced = 0;

extern "C" int entry ()
{
  std::thread worker ([] { produced = 42; });
  worker.join ();
  return produced;
}
""", expect=42, hosted=True, cxx_runtime=True, threads=True),
    # Four threads PARKED on a counting semaphore and woken by ONE release(4), which is
    # the shape the engine's pool shutdown uses for its lanes (`task::stop_lane') and
    # the one the browser run stops in: of two long-parked lane threads, one woke and
    # one did not. The suite could not ask this -- `std_thread' joins a thread that runs
    # to completion and never blocks, so no row covered a thread resumed by a notify.
    #
    # It reports rather than hangs, deliberately: the threads are detached and the main
    # thread polls a counter to a deadline, because `probe_status' runs node with no
    # timeout and a join on a thread that is never woken would hang the whole table
    # instead of printing a row. A lost wake comes back as "wrong result".
    Probe("semaphore_release_n", CXXFLAGS, """
#include <atomic>
#include <chrono>
#include <semaphore>
#include <thread>

static constexpr int parked = 4;

static std::counting_semaphore<> gate{ 0 };
static std::counting_semaphore<> at_gate{ 0 };
static std::atomic<int> woke{ 0 };

extern "C" int entry ()
{
  for (int i = 0; i < parked; ++i) {
    std::thread ([] {
      at_gate.release ();
      gate.acquire ();
      woke.fetch_add (1);
    }).detach ();
  }
  for (int i = 0; i < parked; ++i) {
    at_gate.acquire ();
  }
  std::this_thread::sleep_for (std::chrono::milliseconds (200));
  gate.release (parked);
  const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (5);
  while (woke.load () < parked && std::chrono::steady_clock::now () < deadline) {
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }
  return woke.load () == parked ? 42 : woke.load ();
}
""", expect=42, hosted=True, cxx_runtime=True, threads=True),
    # The same four parked threads woken by four SEPARATE release() calls, the shape
    # `task::pool_shutdown' uses for its workers -- the half of the engine's shutdown
    # that does wake every thread. Kept beside the row above because the pair is what
    # discriminates: both green means the lost wake is not in release's notify count and
    # the engine is where to look.
    Probe("semaphore_release_one_each", CXXFLAGS, """
#include <atomic>
#include <chrono>
#include <semaphore>
#include <thread>

static constexpr int parked = 4;

static std::counting_semaphore<> gate{ 0 };
static std::counting_semaphore<> at_gate{ 0 };
static std::atomic<int> woke{ 0 };

extern "C" int entry ()
{
  for (int i = 0; i < parked; ++i) {
    std::thread ([] {
      at_gate.release ();
      gate.acquire ();
      woke.fetch_add (1);
    }).detach ();
  }
  for (int i = 0; i < parked; ++i) {
    at_gate.acquire ();
  }
  std::this_thread::sleep_for (std::chrono::milliseconds (200));
  for (int i = 0; i < parked; ++i) {
    gate.release ();
  }
  const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (5);
  while (woke.load () < parked && std::chrono::steady_clock::now () < deadline) {
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }
  return woke.load () == parked ? 42 : woke.load ();
}
""", expect=42, hosted=True, cxx_runtime=True, threads=True),
    # A compare-exchange that FAILS: the exchange does not happen, the expected value is
    # updated to what was found, and memory is left alone. Nothing else covered the
    # failing path -- every other atomic row here succeeds.
    #
    # **It is the regression test for ASM_FEATURES**, and that is its load-bearing job:
    # it is the only probe whose own translation unit emits an atomic instruction, so it
    # is the only one that fails when the probe pipeline assembles without `-mattr'
    # ("instruction requires: atomics", measured both ways). Every other thread probe
    # gets its atomics from libstdc++ objects the driver assembled through ASM_SPEC,
    # which is exactly how the pipeline went a whole step passing the flag nowhere and
    # staying green.
    #
    # **It does NOT discriminate the two spellings of `atomic_cmpxchg's memory set**, and
    # that was measured rather than assumed, because the comment it replaces claimed it
    # did. Built against `(set mem desired)' -- the false form, which says memory
    # unconditionally became the desired value -- this returns 7 just the same, with a
    # real `i32.load' in the output, at -O2, for a plain load as well as an atomic one.
    # The reason is the sibling unspec_volatile in the same insn: it makes the whole
    # pattern side-effecting, so no pass uses the second set's claim. The claim is still
    # false and still fixed; what is not available is a row that would catch it coming
    # back. -O2 is kept because the fold would need it if anything ever did look.
    Probe("cmpxchg_fail", CXXFLAGS + " -O2", """
static int cell = 7;

extern "C" int entry ()
{
  int expected = 99;
  if (__atomic_compare_exchange_n (&cell, &expected, 1234, false,
				   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
    return 0;
  if (expected != 7)
    return 1;
  return __atomic_load_n (&cell, __ATOMIC_SEQ_CST);
}
""", expect=7, hosted=True, cxx_runtime=False),
    # A throw caught in the same function, which is the narrowest thing that asks for
    # an unwinder. It carries its own flags because CXXFLAGS turns exceptions and RTTI
    # off for every other probe, and a probe compiled with -fno-exceptions would
    # measure the absence of the question.
    #
    # Worth its own row even though include_vector reaches the same libraries: that one
    # pulls libstdc++'s throwing objects in whether or not the probe throws, so it
    # cannot distinguish "libstdc++ contains throwing code" from "a throw does not
    # work". This one is the second half, and it is the row that separates the
    # terminating unwinder of 0009 from a real one.
    Probe("throw_catch", "-std=c++26", """
#include <stdexcept>

extern "C" int entry ()
{
  try
    {
      throw std::runtime_error ("probe");
    }
  catch (const std::runtime_error &)
    {
      return 7;
    }
  return 0;
}
""", expect=7, hosted=True, cxx_runtime=True,
       blocked=(Blocked(LINKED, "trapped",
                        "0009's terminating unwinder, doing what it says: this links "
                        "and then traps in the abort () inside _Unwind_RaiseException. "
                        "Confirmed to be that abort and not something else by making "
                        "the stub return rather than trap, at which point the probe "
                        "runs and returns 7. So `trapped' is this row's honest status "
                        "until the WebAssembly exception-handling proposal reaches the "
                        "back end -- TARGET_EXCEPT_UNWIND_INFO is UI_NONE, which "
                        "suppresses EH region generation, so nothing downstream has "
                        "ever been asked for a landing pad. The instructions "
                        "themselves are reachable: `eh_instructions' in "
                        "Tools/wasm_dialect_probe.py assembles, links and runs a "
                        "try_table catching a thrown tag. A throw terminating is what "
                        "this target does, and this is the row that says so."),)),
    # A named module with a global initializer, and a second TU that imports it. The
    # narrowest subject for the one thing `import std` cannot ask: an `import std;` TU
    # emits no `_ZGIW` call at all, so until the engine rows arrived nothing in this
    # pipeline had ever assembled a *consumer* of a named module.
    #
    # The initialiser is dynamic on purpose, for two reasons at once. It is what makes
    # the module's initializer non-empty, so `module_determine_import_inits` marks it
    # active and the consumer emits the call this probe is about; and, as in
    # `init_array` above, a value the frontend cannot fold is what separates "the call
    # was emitted" from "the call ran". Freestanding: nothing here needs a libc, and
    # the harness calls `__wasm_call_ctors` itself for a probe that is not a reactor.
    Probe("module_init", CXXFLAGS + " -fmodules", """
export module mi;

static volatile int seed = 9;

export int value = seed * 5;
""", expect=45, second="""
import mi;

extern "C" int entry () { return value; }
"""),
    # A function pointer whose target is instantiated and addressed in one TU and called
    # in another, which is what `funcptr` cannot ask: there both targets are defined in
    # the same unit, so the definition's own signature decides the call site's type and
    # the two cannot disagree. Here the indirect call is in the module interface and the
    # address-take is in its consumer, which is the engine's shape exactly --
    # `Ecs/SystemManifest.cppm` forms `&invoke_annotated_fn<...>` and `Ecs/Scheduler.cpp`
    # calls through it.
    #
    # Two specialisations rather than one, for `funcptr`'s reason: a pointer that
    # resolved to the wrong table index cannot pass by landing on the right function.
    Probe("funcptr_tu", CXXFLAGS + " -fmodules", """
export module fp;

export template <int N> int step (int n) { return n * N; }

export int call_through (int (*fn) (int), int n) { return fn (n); }
""", expect=1421, second="""
import fp;

extern "C" int entry ()
{
  return call_through (&step<2>, 7) * 100 + call_through (&step<3>, 7);
}
"""),
    # `import std`, which is the first thing the engine's own code needs: every file
    # under Engine/Source reaches the standard library that way, and Import/Math.cppm
    # -- the gse.math probe the plan has been pointing at since step 3 -- imports
    # nothing else. Hence this being the last row rather than a nicety: it is the one
    # that says whether the engine-side port can start.
    Probe("import_std", CXXFLAGS + " -fmodules", """
import std;

extern "C" int entry ()
{
  std::vector<int> v {3, 1, 2};
  std::ranges::sort (v);
  return v[0] * 100 + v[1] * 10 + v[2];
}
""", expect=123, hosted=True, cxx_runtime=True, std_module=True),
    # A coroutine, which nothing above measures at all and which is how every system in
    # the engine's scheduler runs. The subject is the narrowest one that reaches the
    # resume this target gets wrong: a task whose `final_suspend` transfers symmetrically
    # to `std::noop_coroutine ()`, which is the arm a task with no continuation takes, and
    # whose resume is a `call_indirect` through a pointer in a frame libstdc++ writes
    # rather than the compiler. Hosted because a coroutine frame is an `operator new`.
    #
    # The returned value is computed from a volatile so the frontend cannot fold it: a
    # constant here would pass whether or not the coroutine ever resumed.
    Probe("coroutine_resume", CXXFLAGS + " -fmodules", """
import std;

struct task
{
  struct promise_type;
  using handle = std::coroutine_handle<promise_type>;

  struct final_awaiter
  {
    bool await_ready () const noexcept { return false; }

    std::coroutine_handle<> await_suspend (handle) const noexcept
    { return std::noop_coroutine (); }

    void await_resume () const noexcept {}
  };

  struct promise_type
  {
    int value = 0;

    task get_return_object () { return task {handle::from_promise (*this)}; }
    std::suspend_always initial_suspend () const noexcept { return {}; }
    final_awaiter final_suspend () const noexcept { return {}; }
    void return_value (int v) { value = v; }
    void unhandled_exception () {}
  };

  handle h;
};

static volatile int seed = 6;

static task make () { co_return seed * 7; }

extern "C" int entry ()
{
  task t = make ();
  t.h.resume ();
  const int out = t.h.promise ().value;
  t.h.destroy ();
  return out;
}
""", expect=42, hosted=True, cxx_runtime=True, std_module=True),
    # A float and a double atomic whose operands live in float registers, which only an
    # optimised build arranges: at -O0 every value goes through memory and is loaded as
    # the integer the pattern wants, so this row is -O2 or it measures nothing. Store,
    # exchange and a compare-exchange in both widths, because the engine's rejected
    # units between them hit all three. Patch 0020 is what turns this green; before it
    # llvm-mc rejected `i64.atomic.store' handed an f64.
    #
    # Every value is arithmetic on a volatile so none of them can be folded to a
    # constant, which would reach the pattern as an integer immediate and pass anyway.
    Probe("float_atomics", CXXFLAGS + " -O2", """
#include <atomic>

static volatile double dseed = 1.5;
static volatile float fseed = 2.5f;
static std::atomic<double> d;
static std::atomic<float> f;

extern "C" int entry ()
{
  const double a = dseed * 4.0;
  const float b = fseed * 2.0f;
  d.store (a);
  f.store (b);
  const double old_d = d.exchange (a + 1.0);
  const float old_f = f.exchange (b + 1.0f);
  double want_d = a + 1.0;
  float want_f = b + 1.0f;
  const bool swapped = d.compare_exchange_strong (want_d, old_d * 5.0)
                       && f.compare_exchange_strong (want_f, old_f * 2.0f);
  return swapped ? static_cast<int> (d.load () + f.load ()) : 0;
}
""", expect=40, hosted=True, cxx_runtime=True, threads=True),
    # The libm half of the parity question: the float transcendental calls the physics
    # orientation path makes, over solver-domain inputs, printed as bits so
    # Tools/libm_probe/run.py can diff them against the same file built for mingw. At
    # -O3 because that is the arm the gate measures.
    Probe("libm_parity", CXXFLAGS + " -O3", "", expect=42, hosted=True, cxx_runtime=True,
          sources=("Tools/libm_probe/probe.cpp",)),
    # The same probe through the vendored CORE-MATH routines gse::libm forwards to, so
    # run.py --coremath can show both arms agree once they share one implementation.
    Probe("libm_parity_coremath", CXXFLAGS + " -O3 -DGSE_LIBM_COREMATH", "", expect=42,
          hosted=True, cxx_runtime=True, sources=("Tools/libm_probe/probe.cpp",)),
    # The engine's own code, which everything above exists to make possible. Five rows
    # rather than one, each the deepest slice that still answers a single question, so
    # that a red names the module it is in rather than "the engine does not compile".
    #
    # `gse.meta` first because it is the engine's reflection-heaviest code: fourteen
    # partitions of P2996 over annotations, fields, enums and dispatch, against a
    # target where `reflection' above only measures the core language. This is the row
    # that says the engine's reflection runs on wasm32, and it is green.
    Probe("engine_meta_parts", ENGINE_CXXFLAGS, "",
          hosted=True, cxx_runtime=True, std_module=True, partitions_of="gse.meta"),
    Probe("engine_assert", ENGINE_CXXFLAGS, "",
          hosted=True, cxx_runtime=True, std_module=True, closure_of="gse.assert"),
    # The whole of gse.meta, `Hash.cppm` included -- the file the previous snapshot
    # would not take.
    Probe("engine_meta", ENGINE_CXXFLAGS, "",
          hosted=True, cxx_runtime=True, std_module=True, closure_of="gse.meta"),
    # The row this step exists for: nineteen partitions of unit-typed math, reflection
    # and contracts, compiled for a 32-bit target for the first time. `Units.cppm`
    # carries its own static_asserts over dimensioned arithmetic, so a green here is
    # already more than "it parsed".
    Probe("engine_math", ENGINE_CXXFLAGS, "", hosted=True, cxx_runtime=True,
          std_module=True, closure_of="gse.math"),
    # And the one that computes. 350 from a unit conversion, 5 from a vec3 magnitude,
    # 6 from a 4x4 float multiply -- the last of which is the only path through
    # `simd::mul_mat4`, whose portable arm has no x86 original to fall back on. A
    # compile-only row could not have caught 0012's constant miscompile, and this
    # family of code is where a repeat of it would land.
    Probe("engine_math_run", ENGINE_CXXFLAGS, """
import std;

import gse.math;

extern "C" int entry ()
{
  const gse::length distance = gse::meters (3.f) + gse::centimeters (50.f);
  const gse::vec3<gse::length> leg = gse::meters (3.f, 4.f, 0.f);
  const gse::mat4f product = gse::mat4f (2.f) * gse::mat4f (3.f);

  return static_cast<int> (distance.as<gse::meters> () * 100.f)
         + static_cast<int> (gse::magnitude (leg).as<gse::meters> ())
         + static_cast<int> (product[0][0]);
}
""", expect=361, hosted=True, cxx_runtime=True, std_module=True,
         closure_of="gse.math",
         # gse.assert's out-of-line symbols, which `Quantity.cppm` reaches through the
         # assert templates. Terminating rather than reporting, the same bargain 0009's
         # unwinder makes: what this row measures is the math, and a real reporter is a
         # host port.
         #
         # `handle_contract_violation` is the fourth and is not an assert symbol at all:
         # `-fcontracts` makes every contract_assert in the engine's math call it, and
         # `Source/Assert/Assert.cpp` is where the engine defines it. It is stubbed here for
         # the same reason as the other three and discovered the same way -- as an
         # undefined symbol at link, once 0013 let this row reach the linker.
         second="""
module gse.assert;

import std;

auto gse::install_fatal_reporter (fatal_reporter) -> void {}

auto gse::fatal_exit (int code) noexcept -> void { std::_Exit (code); }

auto gse::assert_fail (std::source_location, std::string_view) noexcept -> void
{
  std::abort ();
}

extern "C++" auto handle_contract_violation (const std::contracts::contract_violation &)
  -> void
{
  std::abort ();
}
"""),
)

def modes_of(probe: Probe) -> tuple:
    """The pipelines a probe is measured through, which follow from `expect` alone."""
    return (COMPILE,) if probe.expect is None else (LINKED,)


# A `Blocked` naming a mode its probe is never measured through excuses nothing and reads
# as coverage, which is the blindfold the type exists to prevent. Catching it here rather
# than letting it pass as an absent row is what keeps the next person who adds a known-red
# probe from writing one by accident.
for _probe in PROBES:
    for _blocked in _probe.blocked:
        if _blocked.mode not in modes_of(_probe):
            raise SystemExit(f"probe {_probe.name}: not measured in mode {_blocked.mode}")


def configure_args(prefix: Path, msys_root: Path, sysroot: Path) -> list:
    """The cross configuration: a compiler, libgcc and libstdc++, against wasi-libc.

    Every --disable below names a target library that has no wasm32 port to build.
    Dropping any of them makes configure descend into a library that cannot be built
    yet. --disable-libstdcxx is NOT among them any more, which is the point of this
    step: libstdc++ cannot be configured --without-headers, because its feature tests
    compile and link against a libc, so the sysroot has to come first.

    There is deliberately no --with-native-system-header-dir, even though the sysroot's
    headers are not where GCC looks by default: on an MSYS2 host a bare `/include'
    reaches the *compile* as C:/...msys64/include, because the argument converter reads
    it as a POSIX path in a -D. Patch 0006 states it in the two places that have to
    agree instead -- config.gcc for the Makefile and wasm.h for the preprocessor --
    which is how config/mingw handles the same bug.

    --with-as/--with-ld are what let a *target library* build at all, as opposed to a
    probe. A probe stops at `-S` and is handed to llvm-mc by this script, so it never
    needs the driver to find an assembler; libgcc compiles to an object through the
    driver, which looks for `as` in the -B directory and finds the build tree's
    exec-tool shim. That shim refuses with "$original is unset" unless top-level
    configure was told where the target's assembler is, and no amount of PATH helps,
    because the shim is consulted before PATH. Naming them here is also what keeps the
    driver's own notion of the toolchain and this script's agreeing: one llvm-mc and
    one wasm-ld, named once.

    AR_FOR_TARGET and RANLIB_FOR_TARGET are the same problem one step further on: every
    object of libgcc compiles and then `wasm32-unknown-none-ar: command not found' ends
    the build, because there is no binutils for this target and nothing supplies a
    default. llvm-ar writes and indexes a wasm archive, and comes from the same LLVM
    release as the other two.
    """
    llvm_bin = msys_root / "mingw64" / "bin"
    return [
        f"--target={TARGET}",
        f"--prefix={to_msys(prefix)}",
        f"--with-as={to_msys(llvm_bin / 'llvm-mc.exe')}",
        f"--with-ld={to_msys(llvm_bin / 'wasm-ld.exe')}",
        f"AR_FOR_TARGET={to_msys(llvm_bin / 'llvm-ar.exe')}",
        f"RANLIB_FOR_TARGET={to_msys(llvm_bin / 'llvm-ranlib.exe')}",
        f"NM_FOR_TARGET={to_msys(llvm_bin / 'llvm-nm.exe')}",
        "--enable-languages=c,c++",
        "--disable-bootstrap",
        "--enable-checking=release",
        f"--with-sysroot={to_msys(sysroot)}",
        # wasi-libc's signal.h is an #error unless this is defined, and libstdc++'s
        # <csignal> includes it unconditionally. -g -O2 is restated because naming
        # these variables replaces their default rather than adding to it, and the
        # value is quoted here rather than at the join: these arguments go onto a
        # shell command line, and the only ones with a space in them are these two.
        #
        # -D_Thread_local=__thread is load-bearing on the threads variant rather than
        # the inert leftover step 17 left it as: see CXX_RUNTIME_DEFINES, which states
        # the same thing for the compiles that happen outside the library build.
        "CFLAGS_FOR_TARGET='-g -O2 -D_WASI_EMULATED_SIGNAL'",
        "CXXFLAGS_FOR_TARGET='-g -O2 -D_WASI_EMULATED_SIGNAL "
        "-D_Thread_local=__thread'",
        # The threads variant of the sysroot has pthreads, and this is what makes
        # libstdc++ use them: the gthread model decides its atomicity and locking
        # implementation, and with no model it declares no std::mutex at all -- six of
        # step 18's eight rejected engine units. libgcc/config.host's `wasm*-*-*' arm
        # names no thread_file, so `posix' here is what reaches gthr-posix.h, which
        # includes the sysroot's own pthread.h.
        #
        # This argument is the build directory's cache key, so changing it discards the
        # directory and rebuilds all-gcc, libgcc and libstdc++. That is why step 19 did
        # every cheap measurement before touching this line.
        "--enable-threads=posix",
        "--disable-shared",
        # libgcov wraps fork and the six exec functions, and compiles those
        # wrappers whenever a libc is present at all -- wasi-libc has no process
        # primitives, so they are undeclared and libgcc does not build. There is no
        # way to drop them from a t- fragment: libgcc/Makefile.in includes
        # tmake_file at :382 and assigns LIBGCOV_INTERFACE at :923, so the
        # assignment wins. Coverage instrumentation for a browser build is a
        # question for much later anyway.
        "--disable-gcov",
        "--disable-libssp",
        "--disable-libatomic",
        "--disable-libgomp",
        "--disable-libquadmath",
        "--disable-libvtv",
        "--disable-multilib",
        "--disable-nls",
        "--disable-werror",
        # --with-gxx-libcxx-include-dir used to be here, to make the `-stdlib=libc++`
        # that wasm.h forced into CC1PLUS_SPEC a recognized option. Patch 0006 removes
        # that spec instead: a compiler that builds libstdc++ should not be telling the
        # driver to look for libc++, and with the spec gone the option it needed is not
        # needed either.
        "--with-gmp=/mingw64",
        "--with-mpfr=/mingw64",
        "--with-mpc=/mingw64",
        "--with-isl=/mingw64",
    ]


def fetch_source(msys_root: Path, workdir: Path, snapshot: str, cache: Path) -> Path:
    src = workdir / f"gcc-17-{snapshot}"
    if (src / "gcc" / "config.gcc").exists():
        print(f"Using existing source tree {src}")
        return src
    tarball = cache / f"gcc-17-{snapshot}.tar.xz"
    if not tarball.exists():
        cache.mkdir(parents=True, exist_ok=True)
        url = SNAPSHOT_URL.format(snapshot=snapshot)
        print(f"Downloading {url}")
        bash(msys_root, f"curl -fL -o {to_msys(tarball)} {url}")
    workdir.mkdir(parents=True, exist_ok=True)
    print(f"Extracting {tarball}")
    bash(msys_root, f"cd {to_msys(workdir)} && tar -xf {to_msys(tarball)}")
    if not (src / "gcc" / "config.gcc").exists():
        raise SystemExit(f"extraction did not produce {src}")
    return src


def stage_sysroot(msys_root: Path, workdir: Path, cache: Path) -> Path:
    """Download and extract the multi-target wasi-sysroot tarball, once.

    Separate from the flattening below because two callers want the *unflattened* tree:
    fetch_sysroot, which copies one target out of it, and wasm_dialect_probe, which
    links a hand-written subject straight against the threads target's archives. Those
    archives are the point of that subject -- it asks whether real pthreads link and
    run -- so it has to reach the originals rather than a flattened copy of whichever
    target this script happens to be configured for.
    """
    staged = workdir / f"wasi-sysroot-{WASI_SYSROOT_VERSION}"
    if (staged / "lib" / WASI_TARGET).is_dir():
        return staged
    tarball = cache / f"wasi-sysroot-{WASI_SYSROOT_VERSION}.tar.gz"
    if not tarball.exists():
        cache.mkdir(parents=True, exist_ok=True)
        url = WASI_SYSROOT_URL.format(major=WASI_SYSROOT_VERSION.split(".")[0],
                                      version=WASI_SYSROOT_VERSION)
        print(f"Downloading {url}")
        bash(msys_root, f"curl -fL -o {to_msys(tarball)} {url}")
    workdir.mkdir(parents=True, exist_ok=True)
    print(f"Extracting {tarball}")
    bash(msys_root, f"rm -rf {to_msys(staged)} && cd {to_msys(workdir)} "
                    f"&& tar -xzf {to_msys(tarball)}")
    if not (staged / "lib" / WASI_TARGET).is_dir():
        raise SystemExit(f"extraction did not produce {staged}/lib/{WASI_TARGET}")
    return staged


# The `-mattr=' value inside the backend's ASM_SPEC, which is the authority on what
# the driver tells the assembler. Required to be there and to be exactly one.
ASM_SPEC_MATTR = re.compile(r'"-triple=[^"]*-mattr=([^ "]+)')


def check_asm_features(src: Path) -> None:
    """Refuse to measure when ASM_FEATURES and the backend's ASM_SPEC disagree.

    The two have to say the same thing: the driver assembles libgcc and libstdc++
    through ASM_SPEC, and this script assembles every probe itself. Where they differ,
    the probe table reports on an assembler no engine object was ever built with, and
    the failure is a red row whose cause is the harness -- which is exactly the shape
    of step 18c's ENGINE_CXXFLAGS bug and of this check's own origin: the probe
    pipeline passed no -mattr at all for a whole step, and nothing caught it because no
    probe's own translation unit happened to emit an atomic.

    Checked against the patched source rather than the built compiler because that is
    where the string lives, and because a mismatch should stop the run before an hour
    of building rather than after it.
    """
    header = src / "gcc" / "config" / "wasm" / "wasm.h"
    found = ASM_SPEC_MATTR.findall(header.read_text(encoding="utf-8"))
    if found == [ASM_FEATURES]:
        return
    raise SystemExit(
        f"{header}: ASM_SPEC names -mattr={found!r} while this script assembles probes "
        f"with -mattr={ASM_FEATURES!r}. They have to agree, or the probe table measures "
        "an assembler no engine object is built with; see ASM_FEATURES.")


def sysroot_stamp(sysroot: Path) -> tuple:
    """A flattened sysroot's stamp file and the inputs it has to carry.

    Shared with --probe, which refuses a sysroot these do not match rather than
    re-flattening one: a second spelling of the inputs string would let that arm
    measure against a sysroot `fetch_sysroot` would have discarded.
    """
    return (sysroot / ".wasi-sysroot-version",
            f"{WASI_SYSROOT_VERSION}\n{WASI_TARGET}\n{SYSROOT_EDITS}")


def fetch_sysroot(msys_root: Path, workdir: Path, cache: Path) -> Path:
    """Lay the wasi-libc sysroot out the way GCC expects to find one.

    The release tarball is multi-target: headers under include/wasm32-wasip1 and
    libraries under lib/wasm32-wasip1, with wasip2, wasip3 and a threads variant
    beside them. GCC looks for one set of headers and one library directory, so the
    chosen target is flattened into <sysroot>/include and <sysroot>/lib/wasm32-wasi.

    That last directory name is not a tidier spelling of the same thing: wasm.h's
    STANDARD_STARTFILE_PREFIX_1 is "/lib/wasm32-wasi/", so it is where the driver
    looks for crt1.o, and renaming it would mean patching the backend instead.
    """
    sysroot = workdir / "sysroot"
    # Keyed on the pinned version rather than on the sysroot being there at all.
    # WASI_SYSROOT_VERSION is load-bearing -- which of wasi-libc's headers are
    # C++-clean depends on it -- so bumping it against an existing workdir would
    # otherwise go on measuring the previous one, and report that as the new pin.
    stamp, inputs = sysroot_stamp(sysroot)
    if not stale(stamp, inputs):
        print(f"Using existing sysroot {sysroot} ({WASI_SYSROOT_VERSION})")
        return sysroot
    staged = stage_sysroot(msys_root, workdir, cache)
    print(f"Flattening {WASI_TARGET}")
    bash(msys_root, f"rm -rf {to_msys(sysroot)} "
                    f"&& mkdir -p {to_msys(sysroot)}/lib "
                    f"&& cp -r {to_msys(staged)}/include/{WASI_TARGET} {to_msys(sysroot)}/include "
                    f"&& cp -r {to_msys(staged)}/lib/{WASI_TARGET} {to_msys(sysroot)}/lib/wasm32-wasi")
    if not (sysroot / "include" / "stdio.h").exists():
        raise SystemExit(f"flattening did not produce {sysroot}/include/stdio.h")
    declare_setjmp(sysroot)
    record(stamp, inputs)
    return sysroot


# The guard that has to go, as it stands in wasi-libc 32.0's setjmp.h.
SETJMP_GUARD = re.compile(
    r"#if !defined\(__wasm_exception_handling__\)\n#error Setjmp/longjmp support[^\n]*\n"
    r"#endif\n")


def declare_setjmp(sysroot: Path) -> None:
    """Let the flattened setjmp.h declare setjmp, without defining it.

    wasi-libc refuses to declare setjmp at all unless the wasm exception-handling
    proposal is on: the header is a bare #error. That blocks far more than setjmp.
    libstdc++ precompiles <csetjmp> into stdc++.h.gch, and src/c++23/std.cc.in is
    `#include <bits/stdc++.h>', so the #error stops libstdc++ building and stops the
    `std' module existing -- over a function nothing in either one calls.

    Removing the guard leaves `setjmp' declared and undefined, which is the literal
    truth for a wasi-libc built without the proposal: there is no setjmp.o in this
    sysroot, so code that really calls it fails at link, naming the symbol. Nothing
    here defines __wasm_exception_handling__, because that would be a claim about a
    capability the backend does not have -- it would get this header past the
    preprocessor and then produce a module that silently does not unwind.

    The pattern is required rather than optional: a release that writes the guard
    differently must fail here and be looked at, not quietly keep an #error that the
    caller then reads as the edit having been applied.
    """
    header = sysroot / "include" / "setjmp.h"
    text = header.read_text(encoding="utf-8")
    edited, n = SETJMP_GUARD.subn("", text)
    if n != 1:
        raise SystemExit(
            f"{header}: the exception-handling guard this edit removes is not there in "
            f"the shape it is expected in. wasi-libc {WASI_SYSROOT_VERSION} may have "
            "changed it; see declare_setjmp.")
    with open(header, "w", encoding="utf-8", newline="\n") as f:
        f.write(edited)
    print(f"Patched {header}: setjmp declared, still undefined")


def stale(stamp: Path, inputs: str) -> bool:
    """Whether an artifact on disk was produced from inputs other than the current ones.

    Everything expensive here is cached by existing: the source tree, the sysroot, the
    build directory. Existence cannot say *which* inputs produced it, and each of those
    has an input that is both load-bearing and easy to change -- the pinned sysroot
    version, a re-cut local patch, a new configure argument. Changing one leaves an
    artifact that looks finished and measures something else, silently, which is the
    worst failure this script has available to it.

    So each artifact records what it was built from and compares. A missing stamp beside
    an existing artifact counts as stale too: that is exactly the case that cannot be
    told apart from "built correctly, before anything recorded this", and treating it as
    stale costs one rebuild rather than a wrong measurement.
    """
    return not stamp.exists() or stamp.read_text(encoding="utf-8") != inputs


def record(stamp: Path, inputs: str) -> None:
    """Note what an artifact was just built from, for `stale` to read next time."""
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(inputs, encoding="utf-8")


# The vendored series, in application order, and the testsuite patch that is not part
# of building a compiler. Module-level rather than a local of apply_patches because
# --probe verifies the same series without applying it, and a second copy of this
# list is a list that can disagree with the one that did the applying.
PATCH_NAMES = ("0001-wasm-New-backend.patch", "0002-local-Windows-host-portability.patch",
               "0005-local-emit-llvm-mc-s-assembly-dialect.patch",
               "0006-local-sysroot-and-libstdcxx-host.patch",
               "0007-local-one-only-and-aliases.patch",
               "0008-local-import-signatures.patch",
               "0009-local-terminating-unwinder.patch",
               "0010-local-weak-references.patch",
               "0011-local-multi3.patch",
               "0012-local-host-wide-int-operands.patch",
               "0013-local-module-initializer-decl.patch",
               "0014-local-personality-accessors.patch",
               "0015-local-native-tls.patch",
               "0016-local-atomics.patch",
               "0017-local-gthr-no-weak-refs.patch",
               "0018-local-called-function-decls-and-muloti4.patch",
               "0019-local-noop-coroutine-frame-signature.patch",
               "0020-local-float-atomic-operands.patch")
TESTS_PATCH = "0003-wasm-Adjust-tests.patch"


def patch_names(with_tests: bool) -> list:
    """The series a run of this script expects to be applied to its source tree."""
    return list(PATCH_NAMES) + ([TESTS_PATCH] if with_tests else [])


def patch_stamp(src: Path, patches: Path, name: str) -> tuple:
    """A patch, its stamp file, and the digest that stamp has to carry.

    One place rather than two: the digest is what `apply_patches` records and what
    --probe checks, and if those two ever computed it differently the check would
    pass on a tree the applier would have rejected.
    """
    patch = patches / name
    if not patch.exists():
        raise SystemExit(f"missing vendored patch {patch}")
    return (patch, src / f".applied-{name}",
            hashlib.sha256(patch.read_bytes()).hexdigest())


def apply_patches(msys_root: Path, src: Path, patches: Path, with_tests: bool) -> None:
    for name in patch_names(with_tests):
        # Keyed on the patch's bytes, not just its name: `0005` and `0006` are ours and
        # get re-cut, and a stamp that only records "applied" would leave the tree
        # carrying the previous revision while every later step reports on it. Unlike
        # the sysroot and the build directory this cannot be repaired by rebuilding --
        # re-applying an edited patch to an already-patched tree fails in a way that
        # reads like a rejected hunk -- so it refuses and names the fix instead.
        patch, stamp, digest = patch_stamp(src, patches, name)
        if not stale(stamp, digest):
            print(f"{name} already applied")
            continue
        if stamp.exists():
            raise SystemExit(
                f"{name} has changed since it was applied to {src}.\n"
                f"Delete that source tree and re-run; it will be re-extracted and the "
                f"current patches applied to it.")
        # --batch: without it patch asks "Reversed (or previously applied) patch
        # detected! Assume -R?" and, with no tty, answers no and *skips the file* while
        # still failing — which reads like a rejected hunk rather than a re-run.
        bash(msys_root, f"cd {to_msys(src)} && patch -p1 --batch --no-backup-if-mismatch -i {to_msys(patch)}")
        record(stamp, digest)


class Toolchain(NamedTuple):
    """Where the things a probe needs live.

    Grouped because they are all paths and are handed to one function: a positional
    call of `(msys_root, gcc_dir, libgcc_dir)` conceals which is which and permits a
    valid-looking swap, while the field names make the one call site read.
    """
    msys_root: Path
    gcc_dir: Path
    libgcc_dir: Path
    sysroot: Path
    libstdcxx_dir: Path
    libstdcxx_include_dir: Path
    libsupcxx_include_dir: Path
    std_module_sources: tuple

    def near(self, path: Path) -> str:
        """PATH as the run tools can open it: relative to where they are invoked.

        xg++, llvm-mc and wasm-ld are mingw binaries and cannot open the /c/... paths
        an MSYS2 shell hands them, so nothing on a command line here may be absolute.
        """
        return os.path.relpath(path, self.gcc_dir).replace("\\", "/")


def target_cxxflags(tools: Toolchain, hosted: bool, cxx_runtime: bool) -> str:
    """The include flags a compile for this target needs, in one place.

    One place rather than two, because a second copy of them is the likeliest way to
    turn a green row red for a reason that is not the target -- a compile that cannot
    see <vector> fails the same way whatever state libstdc++ is in. Every probe compile
    calls this.

    A freestanding compile keeps -nostdinc, which is what makes it a measurement of the
    compiler rather than of the sysroot; a hosted one is asking about the sysroot, so it
    is simply allowed to see it. No --sysroot is passed: the configured one is what the
    target libraries were built against, and naming a second here would let the two
    disagree without saying so.

    libstdc++'s headers are named explicitly, because nothing here runs `make install`
    and the driver's default C++ include directory is under the *prefix*. Four
    directories, each carrying headers the others do not, and every one of them found by
    a probe failing without it: the target-specific one carries c++config.h, the staged
    one carries most headers, `backward' carries <strstream>, which `std.cc' includes,
    and libsupc++ carries the ones belonging to the ABI runtime rather than to the
    library -- <compare> among them, reached by the first <vector>. An installed
    toolchain has all four on the default path, which is what makes their absence here
    look like a target problem instead of a missing -I. -nostdinc++ ahead of them keeps
    an installed copy, if one ever appears under the prefix, from shadowing the one just
    built.

    The engine's own include directory is last and is NOT conditional, which is the one
    departure from the paragraph above. It belongs here for the reason this function
    exists: `gse.moodycamel` is the first unit the compiler is ever offered that
    `#include`s a vendored header, and a sweep that can see it while the five engine
    probes cannot is flag divergence between the two paths -- 18c's bug, which step 18
    paid for in ten misattributed red rows. It cannot be gated on `hosted` or
    `cxx_runtime`, because three of those probes are freestanding, and it does not weaken
    what `-nostdinc` measures: this is a directory under Engine, not the target's sysroot,
    and a probe that includes nothing from it is compiled exactly as before.
    """
    flags = [] if hosted else ["-nostdinc"]
    if cxx_runtime:
        include_dir = tools.libstdcxx_include_dir
        flags += ["-nostdinc++",
                  f"-I{tools.near(include_dir / TARGET)}",
                  f"-I{tools.near(include_dir)}",
                  f"-I{tools.near(include_dir / 'backward')}",
                  f"-I{tools.near(tools.libsupcxx_include_dir)}",
                  CXX_RUNTIME_DEFINES]
    flags.append(f"-I{tools.near(ENGINE_INCLUDE_DIR)}")
    return " ".join(flags)


def compile_unit(tools: Toolchain, flags: str, stem: str, source: str) -> bool:
    """Compile one translation unit to assembly in the probe directory.

    Assembly rather than an object: llvm-mc is the assembler, and stopping at -S is what
    separates "the compiler accepted this" from every later stage. The directory is
    fixed because the module cache is keyed on the working directory -- a unit compiled
    anywhere else writes a .gcm the next unit cannot find.
    """
    return bash(tools.msys_root,
                f"cd {to_msys(tools.gcc_dir)} && "
                f"./xg++ -B. -S {flags} -o {stem}.s {source}",
                check=False).returncode == 0


def build_std_module(tools: Toolchain, flags: str) -> Optional[str]:
    """Compile libstdc++'s `std` module interface, or name how it failed.

    The `std` module has to exist as a .gcm before anything can import it, and libstdc++
    ships std.cc as a source file for exactly this reason. It is compiled in the probe
    directory rather than once up front because the cache is keyed on the working
    directory. Its own object is discarded: everything the module contributes at link
    time is already in libstdc++.a, and -S leaves nothing to link anyway.

    The interface is concatenated here from the two source-tree `.in' files, which is
    exactly what src/c++23/Makefile's own `std.cc' rule does, and NOT read from the
    generated std.cc in the build tree. That one is rewritten when the module fails to
    compile: `std.lo's recipe answers a failure with `echo > std.cc.tmp && mv
    std.cc.tmp std.cc' so that the library still builds without a module initialiser.
    Reading it therefore measured the build's recovery rather than the target, and which
    of the two a run saw depended on whether `stamp-modules-bits' copied the file before
    or after the emptying -- so one re-run of make turned this row's red from `std module
    failed', with the seven names on it, into a bare `compile failed' about a .gcm that
    was never written.
    """
    parts = " ".join(tools.near(part) for part in tools.std_module_sources)
    if bash(tools.msys_root,
            f"cd {to_msys(tools.gcc_dir)} && cat {parts} > std-module.cc",
            check=False).returncode != 0:
        return "std module sources missing"
    if not compile_unit(tools, flags, "std-module", "std-module.cc"):
        return "std module failed"
    return None


def probe_status(tools: Toolchain, probe: Probe, mode: str) -> str:
    """Take one probe through the pipeline, and name the stage it reached.

    Stages are reported separately because a run probe can fail at any of half a dozen
    points, and "FAILED" alone sends the next reader back to the shell to find out
    which one.
    """
    stem = f"probe-{probe.name}"
    # (output stem, source as the compiler can open it), in compile order. Engine
    # sources are compiled where they sit rather than copied here: a copy would be a
    # second statement of the file, and the one failure mode this probe exists to find
    # -- a construct the engine really uses meeting a 32-bit target -- is exactly the
    # one a stale copy would hide. Their output stems are indexed because two engine
    # modules may own files of the same name.
    sources = probe.sources
    if probe.closure_of:
        sources = graph().closure(probe.closure_of)
    elif probe.partitions_of:
        sources = graph().paths(graph().partitions_of(probe.partitions_of))
    units = [(f"{stem}-{i:02d}", tools.near(REPO_ROOT / source))
             for i, source in enumerate(sources)]
    written = [(stem, probe.body)]
    if probe.second:
        written.append((f"{stem}-b", probe.second))
    for unit, body in written:
        if not body:
            continue
        with open(tools.gcc_dir / f"{unit}.cc", "w", encoding="utf-8", newline="\n") as f:
            f.write(body.lstrip())
        units.append((unit, f"{unit}.cc"))
    msys_root = tools.msys_root
    cd = f"cd {to_msys(tools.gcc_dir)} && "
    flags = f"{target_cxxflags(tools, probe.hosted, probe.cxx_runtime)} {probe.flags}"
    if probe.std_module:
        failure = build_std_module(tools, flags)
        if failure:
            return failure
    for unit, source in units:
        if not compile_unit(tools, flags, unit, source):
            return f"compile failed: {Path(source).name}"
    if mode == COMPILE:
        return "compiled"

    for unit, _ in units:
        if bash(msys_root, cd + f"llvm-mc -triple={ASM_TRIPLE} "
                                f"-mattr={ASM_FEATURES} -filetype=obj "
                                f"{unit}.s -o {unit}.o", check=False).returncode != 0:
            return "assemble failed"

    binary = f"{stem}.wasm"
    objects = [f"{unit}.o" for unit, _ in units]
    if bash(msys_root, cd + wasm_ld(tools, objects, binary, probe.hosted,
                                    probe.cxx_runtime, probe.threads),
            check=False).returncode != 0:
        return "link failed"
    # --no-warnings: node:wasi is an experimental module and says so on stderr every
    # time the harness is loaded, whether or not the probe uses it. Nothing else here
    # warns, so there is nothing else to lose by silencing it.
    # The harness cannot derive sharedness from the module -- Module.imports reports a
    # memory import's kind but not its flags -- so a threads probe has to say so, or it
    # is handed a plain memory and fails with "memory import has no maximum limit".
    mode = " shared" if probe.threads else ""
    code = bash(msys_root,
                cd + f"node --no-warnings harness.mjs {binary} {probe.expect}{mode}",
                check=False).returncode
    if code == NO_INSTANCE_EXIT:
        return "did not instantiate"
    if code == TRAP_EXIT:
        return "trapped"
    if code != 0:
        return "wrong result"
    return "ran"


def wasm_ld(tools: Toolchain, objects: list, binary: str, hosted: bool,
            cxx_runtime: bool, threads: bool) -> str:
    """The probes' wasm-ld command line, assembled in one place.

    One place for the reason the order itself has: it is load-bearing and invisible, so a
    second copy of it is how a measurement becomes an unresolved symbol. The engine's own
    link is the wasm32 CMake configuration's, and its rule in
    Engine/cmake/toolchains/wasm32-wasi-gcc-rules.cmake is this line's hosted, threaded arm
    with the engine's stack -- the two have to move together.
    """
    # Section collection is left on, which the rwat pipeline could not do: GCC's
    # `__stack_pointer` was an import with no symbol, so wasm-ld used to drop it and
    # every module died on `global.get 0`. Declared with `.globaltype` it has a real
    # symbol, and leaving the default in place is what keeps measuring that.
    #
    # In dependency order, because wasm-ld pulls archive members lazily and scans left
    # to right, so a library must appear before everything it needs. Each archive below
    # depends only on ones further down -- libstdc++ on libsupc++, both on libgcc (and
    # on libc for malloc), libgcc on libc for memcpy and abort. Appending them from
    # separate `if` blocks got this wrong in the other direction and would not have
    # shown up until libstdc++ built, as a `link failed` on a probe whose known red is
    # a compile error.
    #
    # crt1-reactor rather than crt1: the command form defines _start and expects to be
    # the whole program, while a reactor exposes _initialize and leaves the module
    # callable afterwards -- which is what every probe here is, a module whose `entry`
    # the harness calls. It is also what lets one harness run the hosted and
    # freestanding probes alike.
    #
    # -lwasi-emulated-signal is the other half of -D_WASI_EMULATED_SIGNAL: the define
    # gets <csignal> past wasi-libc's #error, and this supplies the stubs it declares.
    #
    # libgcc is on every link line rather than only the probe that needs it. A libcall
    # is emitted by the compiler, not written by the probe, so which probes need it is
    # not a fact the probe list can state -- `-L` plus `-lgcc` costs nothing where it
    # is unused and is the difference between a measurement and an unresolved symbol
    # where it is not.
    wasi_lib = tools.near(tools.sysroot / "lib" / "wasm32-wasi")
    link = []
    if hosted:
        link += [f"{wasi_lib}/crt1-reactor.o"]
    link += objects
    if cxx_runtime:
        # -lstdc++ alone, and deliberately not -lsupc++ beside it: libtool links
        # libsupc++ into libstdc++ as a convenience library, so all 65 of its members
        # -- eh_throw, eh_personality, tinfo, pure, the lot -- are already among
        # libstdc++.a's 199. Naming the second archive as well presents every one of
        # them to the linker twice, and wasm-ld rejects that rather than taking the
        # first: `duplicate symbol: std::type_info::~type_info()' and a dozen
        # `symbol type mismatch' lines, none of which are about the target.
        #
        # This was invisible for as long as libstdc++.a did not exist -- the probe
        # stopped at `unable to find library' before it could double-count anything --
        # so the archive appearing is what turned a wrong link line into a red probe.
        link += [f"-L{tools.near(tools.libstdcxx_dir)}", "-lstdc++"]
    link += [f"-L{tools.near(tools.libgcc_dir)}", "-lgcc"]
    if hosted:
        link += [f"-L{wasi_lib}", "-lc", "-lwasi-emulated-signal"]
    # No -lpthread: on this sysroot libpthread.a is an empty 8-byte archive and every
    # pthread_* API is in libc.a, which is also why 0017 turns the weak references off.
    # What a shared memory costs at the link is below, each flag measured in 19a rather
    # than copied from a tutorial: --shared-memory needs a maximum, and --export-memory
    # is needed because node:wasi binds to `instance.exports.memory' and refuses a
    # module that only imports one.
    if threads:
        link += ["--shared-memory", "--import-memory", "--export-memory",
                 f"--max-memory={HARNESS_MAX_PAGES * PAGE_SIZE}"]
    # --export-if-defined rather than --export for the two entry points: the linker
    # synthesises __wasm_call_ctors only where some object contributed an init-array
    # entry and _initialize only for a reactor, and demanding either unconditionally
    # would turn every probe that does not have one into a link failure.
    #
    # --error-limit=0 because a truncated list is not an inventory. wasm-ld stops at 20
    # errors by default and says so, which the first engine link hit: fifteen undefined
    # symbols reported out of an unknown number, with "too many errors emitted" as the
    # only hint that the measurement was a sample. No probe is affected -- it changes how
    # much is printed when a link fails, never whether it does.
    #
    # TWO DASHES: wasm-ld parses `-error-limit=0` as `-e rror-limit=0`, because
    # single-dash `-e` is the entry symbol. The link then fails with `entry symbol not
    # defined: rror-limit=0` and reports zero undefined symbols, which is what a link
    # that never resolved anything looks like from the outside.
    exports = [f"--export={ENTRY}",
               "--error-limit=0",
               "--export-if-defined=__wasm_call_ctors",
               "--export-if-defined=_initialize",
               "--export-if-defined=__wasm_call_dtors"]
    return (f"wasm-ld {' '.join(link)} -o {binary} --no-entry "
            f"{' '.join(exports)}")


def build_stamp(build: Path, src: Path, prefix: Path, msys_root: Path,
                sysroot: Path) -> tuple:
    """A build directory's stamp file and the inputs it has to carry.

    What it was produced from is the configure arguments *and the source tree*. The tree
    is the one input a snapshot bump changes: there is a tree per snapshot and one build
    dir, configure bakes an srcdir into its Makefiles, and nothing in configure_args
    names it -- so a new --snapshot against an existing build dir would keep Makefiles
    pointing at the previous tree and go on building it. That is a run which looks like
    the new snapshot and measures the old one, the failure `stale' exists to prevent,
    and it cannot be told from a successful bump by anything the build prints.

    Shared with --probe for the same reason the two above are: that arm's whole
    claim to be measuring this toolchain is that these inputs still match, so it has to
    compare the string main would have written, not a second rendering of it.
    """
    return (build / ".configure-args",
            "\n".join([f"srcdir {src.name}"]
                      + configure_args(prefix, msys_root, sysroot)))


def toolchain_for(msys_root: Path, src: Path, build: Path, sysroot: Path) -> Toolchain:
    """Where this build directory's compiler, target libraries and headers ended up.

    Every path is derived from the build dir or the source tree and none of them depend
    on the build having just happened, which is what lets --probe construct the
    same toolchain the full run does without re-walking it.
    """
    return Toolchain(msys_root=msys_root, gcc_dir=build / "gcc",
                     libgcc_dir=build / TARGET / "libgcc",
                     sysroot=sysroot,
                     libstdcxx_dir=build / TARGET / "libstdc++-v3" / "src" / ".libs",
                     libstdcxx_include_dir=build / TARGET / "libstdc++-v3" / "include",
                     libsupcxx_include_dir=src / "libstdc++-v3" / "libsupc++",
                     std_module_sources=(
                         src / "libstdc++-v3" / "src" / "c++23" / "std.cc.in",
                         src / "libstdc++-v3" / "src" / "c++23" / "std-clib.cc.in"))


def verify_workdir(args, msys_root: Path, workdir: Path) -> Toolchain:
    """The toolchain already in --workdir, behind every refusal that keeps it honest.

    The full run exists to answer "does this configuration still work", so it walks fetch,
    patch, configure and three makes before it measures anything. That walk is ~50
    minutes against an already-finished build directory, and six consecutive attempts at
    one measurement died inside it rather than inside the measurement. Nothing in `Toolchain`
    needs the walk to have happened -- every path in it is a location, not a product of
    this run -- so this derives the same paths and hands them back.

    What such an arm cannot do is notice that the build directory was produced from other
    inputs, which is the one way it could print a number from the wrong compiler. So every
    input the full run would have re-established is checked here instead, read-only, and a
    mismatch is a refusal naming the full run as the fix. The checks cost milliseconds;
    being wrong about which compiler a measurement came from has already cost this port
    more than the walk has.

    One function and not a paragraph in `--probe`, because these five refusals ARE the
    claim that the measurement is of the pinned compiler. A copy that drifted would be an
    arm that reports confidently on a toolchain nothing describes.

    `--probe` runs the ones it is named and no others, and says how many of the table
    that was.
    """
    src = workdir / f"gcc-17-{args.snapshot}"
    build = workdir / "build"
    sysroot = workdir / "sysroot"
    full_run = ("build it with `python Tools/build_gcc_wasm.py`, which walks "
                "fetch, patch, configure and the makes this arm skips")

    # The source tree, because Toolchain reads libsupc++'s headers and std.cc.in out of
    # it -- an engine probe's first act is to build the `std' module from those.
    if not (src / "gcc" / "config.gcc").exists():
        raise SystemExit(f"no patched source tree at {src}; {full_run}")

    # The compiler and both target libraries. cc1plus beside xg++ because the driver is
    # a stub without it, and the archives because a compile-mode probe links nothing but
    # would report a half-built libstdc++ as a wasm32 finding about the engine.
    for needed in (build / "gcc" / "xg++.exe", build / "gcc" / "cc1plus.exe",
                   build / TARGET / "libgcc" / "libgcc.a",
                   build / TARGET / "libstdc++-v3" / "src" / ".libs" / "libstdc++.a"):
        if not needed.exists():
            raise SystemExit(f"{needed} is missing, so this workdir holds no finished "
                             f"toolchain; {full_run}")

    stamp, inputs = build_stamp(build, src, args.prefix.resolve(), msys_root, sysroot)
    if stale(stamp, inputs):
        raise SystemExit(f"{build} was configured from other inputs than this script "
                         f"now asks for, so a probe of it would report on a compiler "
                         f"nothing here describes; {full_run}")

    # A refusal rather than a call to fetch_sysroot: its matching path is a print, but
    # its stale path silently downloads and re-flattens a sysroot, which is the kind of
    # long walk this arm exists to not do by surprise.
    sysroot_marker, sysroot_inputs = sysroot_stamp(sysroot)
    if stale(sysroot_marker, sysroot_inputs):
        raise SystemExit(f"{sysroot} was flattened from other inputs than "
                         f"WASI_SYSROOT_VERSION {WASI_SYSROOT_VERSION} / {WASI_TARGET}; "
                         f"{full_run}")

    # The series, verified and not applied. Read-only -- a digest per patch file, no
    # shell -- and it is the one input of the four that rebuilding cannot repair: a
    # re-cut patch leaves a tree that looks patched and carries the previous revision.
    for name in patch_names(args.with_tests):
        _, patch_marker, digest = patch_stamp(src, args.patches.resolve(), name)
        if stale(patch_marker, digest):
            raise SystemExit(f"{name} is not applied to {src} in its current form; "
                             f"{full_run}")

    # Kept rather than skipped: one regex over the backend's wasm.h, and it is exactly
    # the "measured through an assembler no object was built with" failure that a run
    # skipping the build has no other guard against.
    check_asm_features(src)

    tools = toolchain_for(msys_root, src, build, sysroot)
    write_harness(tools.gcc_dir)
    return tools


def run_probes(tools: Toolchain, probes: tuple) -> list:
    """Measure these probes and print the verdict table. Returns the failing rows.

    One function and not a loop per caller: the full run measures the whole table and
    `--probe` measures a named subset, and the part worth getting right is not the
    measuring but the READING of it -- which status counts as a pass in which mode, and
    what a `blocked` row that has started passing is called. A second copy of that
    judgement would be a subset arm that disagreed with the full run about whether the
    same row is green.
    """
    results = [(probe, mode, probe_status(tools, probe, mode))
               for probe in probes for mode in modes_of(probe)]

    print("\n== result ==")
    failed = []
    for probe, mode, status in results:
        blocked = next((b for b in probe.blocked if b.mode == mode), None)
        # What a pass is named in this mode, which is the mode's own last stage: COMPILE
        # stops at the compiler and so passes as `compiled', never as `ran'. Reading
        # "ran" as the only pass reported a *blocked compile-mode row that had started
        # passing* as "FAILED: compiled, not the ... it is blocked on" -- which is how
        # the snapshot bump's two green engine rows first presented, a fixed gap wearing
        # a regression's clothes. The loudest case the table has was the broken one.
        passing = "compiled" if mode == COMPILE else "ran"
        if blocked and status == passing:
            note = f"NOW PASSES - drop its `blocked` entry ({blocked.reason})"
        elif blocked and status == blocked.status:
            note = f"expected red: {status} - {blocked.reason}"
        elif blocked:
            note = f"FAILED: {status}, not the {blocked.status} it is blocked on"
            failed.append(f"{probe.name}/{mode}")
        elif status == passing:
            note = status.upper()
        else:
            note = f"FAILED: {status}"
            failed.append(f"{probe.name}/{mode}")
        print(f"  {probe.name:18} {mode:9} {note}")
    return failed


def probes_only(args, msys_root: Path, workdir: Path, names: list) -> None:
    """Measure named probes against the toolchain already in --workdir.

    The table is otherwise reachable only through the full run, which walks fetch,
    patch, configure and three makes first -- so a single new row cost hours, and the
    whole table has gone unmeasured for stretches because of it. A probe link is
    seconds; the refusals in `verify_workdir` are what make measuring one against an
    existing build directory a claim about the pinned compiler rather than about
    whatever is on disk.

    Unknown names are refused rather than skipped: a typo that measured nothing would
    print an empty table and read as everything asked for being green.
    """
    unknown = [name for name in names if all(p.name != name for p in PROBES)]
    if unknown:
        raise SystemExit(f"no such probe: {', '.join(unknown)}; the table is "
                         f"{', '.join(p.name for p in PROBES)}")
    tools = verify_workdir(args, msys_root, workdir)
    chosen = tuple(p for p in PROBES if p.name in names)
    print(f"\n== probes ({len(chosen)} of {len(PROBES)}, "
          f"toolchain in {tools.gcc_dir.parent} built elsewhere) ==")
    failed = run_probes(tools, chosen)
    print(f"\nCompiler: {tools.gcc_dir / 'xg++.exe'}")
    if failed:
        raise SystemExit(f"probes failed: {', '.join(failed)}")


def main() -> None:
    p = argparse.ArgumentParser(description="Build an experimental wasm32 GCC cross compiler")
    p.add_argument("--snapshot", default=DEFAULT_SNAPSHOT,
                   help=f"GCC 17 weekly snapshot date (default {DEFAULT_SNAPSHOT}, "
                        f"what the engine's own toolchain is built from)")
    p.add_argument("--msys-root", type=Path, default=DEFAULT_MSYS_ROOT)
    p.add_argument("--patches", type=Path, default=REPO_ROOT / "Tools" / "gcc-toolchain" / "wasm")
    p.add_argument("--src-cache", type=Path, default=GENERATED_ROOT / ".gcc-src-cache")
    p.add_argument("--workdir", type=Path, default=GENERATED_ROOT / ".gcc-wasm-build")
    p.add_argument("--prefix", type=Path, default=GENERATED_ROOT / ".gcc-wasm")
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--with-tests", action="store_true",
                   help="Also apply the testsuite patch (not needed to build a compiler)")
    p.add_argument("--reconfigure", action="store_true", help="Discard the build dir and configure again")
    p.add_argument("--probe", nargs="+", metavar="NAME", default=None,
                   help="Measure these probes against the toolchain already in "
                        "--workdir, skipping fetch, patch, configure and the makes. "
                        "Verifies that build directory was produced from the current "
                        "inputs and refuses if not")
    args = p.parse_args()

    msys_root = args.msys_root.resolve()
    workdir = args.workdir.resolve()
    if args.probe is not None:
        # Refused rather than ignored: --reconfigure asks for the build directory to be
        # discarded, which is the one thing this arm cannot do and still have anything
        # to measure.
        if args.reconfigure:
            raise SystemExit("--probe measures the probe table against an existing "
                             "build directory; --reconfigure discards it")
        probes_only(args, msys_root, workdir, args.probe)
        return

    src = fetch_source(msys_root, workdir, args.snapshot, args.src_cache.resolve())
    apply_patches(msys_root, src, args.patches.resolve(), args.with_tests)
    check_asm_features(src)
    sysroot = fetch_sysroot(msys_root, workdir, args.src_cache.resolve())

    # Before configure, not after: configure now names llvm-mc and wasm-ld as the
    # target's assembler and linker, and it probes the assembler it was given.
    print("\n== run tools ==")
    bash(msys_root, f"pacman -S --noconfirm --needed {' '.join(RUN_TOOLS)}", msystem="MSYS")

    build = workdir / "build"
    wanted = configure_args(args.prefix.resolve(), msys_root, sysroot)
    stamp, inputs = build_stamp(build, src, args.prefix.resolve(), msys_root, sysroot)
    # Reconfigure when the arguments change, not only when asked for. A build dir left
    # by an older revision of this script keeps Makefiles that predate whatever
    # configure_args has gained since, and make cannot notice: that is how `--with-as`
    # would be silently missing from an existing tree, and the failure it produces
    # ("there is no 'as' for the target") names neither the cause nor the fix.
    if build.exists() and (args.reconfigure or stale(stamp, inputs)):
        print("\n== discarding a build dir configured from other inputs ==")
        shutil.rmtree(build)
    if not (build / "Makefile").exists():
        build.mkdir(parents=True, exist_ok=True)
        print("\n== configure ==")
        # srcdir must be RELATIVE. Configured through an absolute MSYS path, srcdir
        # becomes /c/Users/..., and gengtype.exe — a mingw binary, not an MSYS one —
        # cannot open those, so s-gtype dies on a header that is plainly there. The
        # vendored PKGBUILD sidesteps this the same way, with ../${_sourcedir}/configure.
        rel_src = os.path.relpath(src, build).replace("\\", "/")
        bash(msys_root, f"cd {to_msys(build)} && {rel_src}/configure {' '.join(wanted)}")
        record(stamp, inputs)

    print("\n== make all-gcc ==")
    bash(msys_root, f"cd {to_msys(build)} && make -O -j{args.jobs} all-gcc")

    # libgcc is the first target library this build has ever descended into, and it
    # has to exist before a probe can call one: a `long long` divide is a call to
    # __divdi3, and without libgcc that is a link error. It is also what the libstdc++
    # work sits on, so its failures belong here rather than there.
    print("\n== make all-target-libgcc ==")
    bash(msys_root, f"cd {to_msys(build)} && make -O -j{args.jobs} all-target-libgcc")

    # The step this whole configuration exists for. Its exit code is read off the
    # probes rather than acted on here: libstdc++ builds in dependency order, so a late
    # failure still leaves a usable library behind, and "which parts of it work" is a
    # question only the probes can answer. Ending the run on make's result would throw
    # that answer away before it was asked.
    print("\n== make all-target-libstdc++-v3 ==")
    if bash(msys_root, f"cd {to_msys(build)} && make -O -j{args.jobs} all-target-libstdc++-v3",
            check=False).returncode != 0:
        print("libstdc++ did not build cleanly; the hosted probes report how far it got")

    tools = toolchain_for(msys_root, src, build, sysroot)
    gcc_dir = tools.gcc_dir
    # Unconditionally, where this used to ask whether any probe carries an `expect`.
    # It always does, and a guard that cannot be false reads as a case someone
    # considered rather than one that cannot arise.
    write_harness(gcc_dir)

    print("\n== probes ==")
    failed = run_probes(tools, PROBES)

    print(f"\nCompiler: {gcc_dir / 'xg++.exe'}")
    if f"{PROBES[0].name}/{COMPILE}" in failed:
        raise SystemExit("the baseline C++ probe failed; the compiler is not usable yet")
    if failed:
        raise SystemExit(f"probes failed: {', '.join(failed)}")


if __name__ == "__main__":
    main()
