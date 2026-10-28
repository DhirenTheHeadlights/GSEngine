"""Measure whether LLVM's WebAssembly assembly dialect can express what GCC's
wasm backend needs, by assembling hand-written subjects with llvm-mc.

It began as one question answered with a measurement instead of an estimate:
`static` and `funcptr` were red through the rwat pipeline because rwat has no data
symbol kind, no segment info and no MEMORY_ADDR or TABLE_INDEX relocations (see
Tools/gcc-toolchain/wasm/README.md), and closing that gap meant either forking rwat
or moving GCC onto llvm-mc's dialect. GCC was moved (patch 0005), so the first three
subjects here are now the record of why — and the next three are the questions that
port had to settle before a line of it was written: whether a `loop` can carry a
result type and a branch can be spelled as a depth, whether a function can be
imported from `env`, and whether a signature may be declared after the call that
needs it. That last one is red, and it is the reason the module header exists.

`init_array` arrived the same way one step later: static constructors did not run,
and how to spell one had to be settled before the hook that emits it was written.
`comdat` and `alias` arrived the same way one step later again, when libstdc++ asked
the target for link-once semantics and for `__attribute__((alias))` and the target had
neither; both are green, so both are wired in the backend rather than worked around.

`weak_undef` and `eh_instructions` are step 12's, and the two are the same question
asked at opposite ends. The first decided where to repair a link that failed on
libitm's `_ITM_*`: green here means a weak *reference* is the target's to emit, so
`0010` emits it, rather than libstdc++ opting out of weak references altogether. The
second asks whether real exceptions are reachable with these tools before any step
estimates them -- it is green, so the WebAssembly exception-handling proposal is
GCC-side work and not a dead end.

Each subject stays because it is cheaper to keep than to re-derive: this is where a
question about the dialect itself gets answered without a GCC build in the way.

Deliberately NOT part of build_gcc_wasm.py's probe table. These are hand-written,
not GCC output, so a row there would read as GCC passing something it does not. The
split also makes this cheap: hand-written .s needs no snapshot, no cc1plus and no
`make all-gcc`, so this runs in seconds where that script runs in an hour.

What it reuses from build_gcc_wasm.py and run_wasm.py is the part that would otherwise
be restated wrong: the node harness text and the three-way instantiate/trap/wrong-result exit
taxonomy, so a result here and a result there mean the same thing.

    python Tools/wasm_dialect_probe.py
"""
import argparse
from pathlib import Path
from typing import NamedTuple

from build_gcc_wasm import ASM_FEATURES, RUN_TOOLS, WASI_TARGET, stage_sysroot
from run_wasm import (ENTRY, HARNESS_MAX_PAGES, NO_INSTANCE_EXIT, PAGE_SIZE, TRAP_EXIT,
                      harness_text)
from msys2 import DEFAULT_MSYS_ROOT, REPO_ROOT, bash, to_msys
from repo_paths import GENERATED_ROOT

# Exactly build_gcc_wasm's tools, reused rather than restated: a relocation proved
# here is only useful if the llvm-mc and wasm-ld that consume GCC's output are the
# same ones, and sharing the tuple is what stops the two pipelines drifting onto
# different LLVM releases. Nothing is added any more -- llvm-mc arrived in that list
# when the GCC pipeline moved onto it.
TOOLS = RUN_TOOLS

TRIPLE = "wasm32-unknown-unknown"

# The reference-text mitigation the scope preferred is unavailable: MSYS2's rustc is
# not rustup-based (its sysroot carries only x86_64-pc-windows-gnu), so there is no
# `core` for wasm32 and `rustc --target wasm32-unknown-unknown --emit asm` cannot
# produce LLVM-authored asm to diff against. Structural read-back stands in for it,
# which is what each subject's `structure` tuple is for.
#
# Each subject names the object facts that must appear for its result to mean what it
# claims. A green run alone would not distinguish "the dialect carries this" from "the
# linker happened to put the right bytes at address zero" -- the exact confusion that
# made step 3's `static` return 2002 look like a codegen bug rather than a missing
# linker. Asserting the relocation by name is what rules that out.


class Subject(NamedTuple):
    """A hand-written LLVM-dialect subject and what it must produce.

    `sources` is a tuple because the collision subject needs two objects linked
    together; a single-source subject is a one-element tuple rather than a separate
    shape, so the pipeline has one path through it.

    `structure` names substrings that must all appear in the first object's
    `wasm-objdump -x`. They are the corroboration, not decoration: see the comment
    above.

    `blocked` carries the exact status a subject is known to fail with, for the
    ones measuring something the dialect turns out NOT to carry. The status is as
    load-bearing as the fact: a blocked subject that fails some other way is a new
    problem wearing a known one's clothes, and one that starts passing is reported
    louder still. Same shape, and the same reasons, as build_gcc_wasm's `Blocked`.

    `asm_flags` is for a subject measuring an instruction set the assembler keeps
    behind a feature flag, which is a result in itself -- a dialect that needs
    `-mattr' for something is a dialect ASM_SPEC has to name it for. Default empty,
    so every other subject stays a measurement of what llvm-mc takes by default.

    `threads` is the same field, and the same single switch, as build_gcc_wasm's
    Probe: a shared linear memory on the link line and a harness that services
    wasi.thread-spawn. It is not separable into "shared memory" and "atomics" because
    wasm-ld does not separate them -- it refuses thread-local storage outright unless
    both the atomics and bulk-memory features are declared, which is how the `tls`
    subject below found that out.
    """
    name: str
    sources: tuple
    expect: int
    structure: tuple
    proves: str
    blocked: str = ""
    asm_flags: str = ""
    threads: bool = False
    # Archives from the threads sysroot, for the one subject that asks whether real
    # pthreads link and run. Named per-subject rather than put on every link line: a
    # subject that does not ask for a libc must not be able to fail because of one,
    # which is the same rule `hosted` follows in the other script.
    sysroot_libs: tuple = ()


SUBJECTS = (
    # The `static` probe, which is red through rwat because a static cannot be placed:
    # rwat emits no data symbol for `(@sym $lo ...)` and has no MEMORY_ADDR relocation
    # for the address to become. Both are what the structure assertion checks for.
    Subject(
        name="static",
        sources=(("static.s", """
	.text
	.globaltype	__stack_pointer, i32

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	lo
	i32.load	0
	i32.const	hi
	i32.load	0
	i32.const	1000
	i32.mul
	i32.add
	end_function

	.type	lo,@object
	.section	.data.lo,"",@
	.globl	lo
	.p2align	2
lo:
	.int32	40
	.size	lo, 4

	.type	hi,@object
	.section	.data.hi,"",@
	.globl	hi
	.p2align	2
hi:
	.int32	2
	.size	hi, 4
"""),),
        expect=2040,
        structure=("R_WASM_MEMORY_ADDR_SLEB", "segment info", "D <lo>", "D <hi>"),
        proves="data symbols, segment info and MEMORY_ADDR relocations",
    ),
    # The `funcptr` probe, red through rwat for the same class of reason one layer
    # further out: a function pointer stored in data needs a relocation in the DATA
    # section, and rwat patches CODE only. TABLE_INDEX_I32 appearing here is therefore
    # the single most load-bearing line this script prints.
    Subject(
        name="funcptr",
        sources=(("funcptr.s", """
	.text
	.tabletype	__indirect_function_table, funcref

	.globl	twice
	.type	twice,@function
twice:
	.functype	twice (i32) -> (i32)
	local.get	0
	i32.const	2
	i32.mul
	end_function

	.globl	thrice
	.type	thrice,@function
thrice:
	.functype	thrice (i32) -> (i32)
	local.get	0
	i32.const	3
	i32.mul
	end_function

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	7
	i32.const	fns
	i32.load	0
	call_indirect	__indirect_function_table, (i32) -> (i32)
	i32.const	7
	i32.const	fns
	i32.load	4
	call_indirect	__indirect_function_table, (i32) -> (i32)
	i32.add
	end_function

	.type	fns,@object
	.section	.data.fns,"",@
	.globl	fns
	.p2align	2
fns:
	.int32	twice
	.int32	thrice
	.size	fns, 8
"""),),
        expect=35,
        structure=("R_WASM_TABLE_INDEX_I32", "R_WASM_TABLE_NUMBER_LEB", "D <fns>"),
        proves="TABLE_INDEX relocations in a data section, and elem-segment synthesis",
    ),
    # Not a port of an existing probe: the hazard rwat carries that nothing measures.
    # rwat emits every symbol global and strong, so two objects each defining an
    # internal `scale` collide. Here they must coexist, which is what binding=local in
    # the structure assertion pins down -- and if binding were ignored this subject
    # fails at the link, loudly, rather than by quietly calling the wrong `scale`.
    Subject(
        name="two_tu",
        sources=(("two_tu_a.s", """
	.text
	.functype	part_b () -> (i32)

	.type	scale,@function
scale:
	.functype	scale (i32) -> (i32)
	local.get	0
	i32.const	3
	i32.mul
	end_function

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	14
	call	scale
	i32.const	7
	i32.add
	call	part_b
	i32.add
	end_function
"""), ("two_tu_b.s", """
	.text
	.type	scale,@function
scale:
	.functype	scale (i32) -> (i32)
	local.get	0
	i32.const	100
	i32.mul
	end_function

	.globl	part_b
	.type	part_b,@function
part_b:
	.functype	part_b () -> (i32)
	i32.const	2
	call	scale
	end_function
""")),
        expect=249,
        structure=("binding=local",),
        proves="local binding, so same-named internal symbols in two objects coexist",
    ),
    # The shape the backend emits for every function: one `block` per basic block
    # inside one value-returning `loop`, dispatched on a control local. Written here
    # in the flat dialect because three things about it were unknown and all three
    # decide the framing -- whether a `loop' can carry a result type at all, whether
    # `br_table' takes the brace list, and whether a branch target can be spelled as
    # a depth counted from the branch rather than as a label. The depths below are
    # the ones wasm-asm.cc computes: 2 from inside block $1, 1 from inside block $2.
    Subject(
        name="blocktype",
        sources=(("blocktype.s", """
	.text
	.globaltype	__stack_pointer, i32

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	.local	i32, i32
	i32.const	0
	local.set	0
	loop	i32
	block
	block
	block
	local.get	0
	br_table	{0, 1, 2}
	end_block
	i32.const	7
	local.set	1
	i32.const	1
	local.set	0
	i32.const	1
	br_if	2
	end_block
	local.get	1
	i32.const	20
	i32.add
	local.set	1
	i32.const	2
	local.set	0
	br	1
	end_block
	local.get	1
	return
	end_loop
	end_function
"""),),
        expect=27,
        structure=("F <entry>",),
        proves="a result-typed loop, br_table's list form, and branch-by-depth",
    ),
    # Whether a callee's signature may be declared after the call that needs it.
    # It may not, and that is why this is the one red subject here: the module
    # header has to carry every `.functype' ahead of the body, which is what keeps
    # 0002's body buffering load-bearing rather than merely convenient.
    Subject(
        name="late_functype",
        sources=(("late_a.s", """
	.text
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	14
	call	part_b
	i32.add
	end_function

	.functype	part_b () -> (i32)
"""), ("late_b.s", """
	.text
	.globl	part_b
	.type	part_b,@function
part_b:
	.functype	part_b () -> (i32)
	i32.const	5
	end_function
""")),
        expect=19,
        structure=("F <entry>",),
        proves="nothing: a signature declared after its call is rejected outright",
        blocked="assemble failed",
    ),
    # An imported function, which is what the three hardcoded `(import "env" ...)'
    # lines in the annotated dialect have to become. The memory and the table are
    # the linker's to synthesise and need no import at all; a function that is meant
    # to come from the host, rather than from another object, needs these two
    # directives and is otherwise an ordinary undefined symbol.
    Subject(
        name="env_import",
        sources=(("env_import.s", """
	.text
	.functype	helper (i32) -> (i32)
	.import_module	helper, env
	.import_name	helper, helper

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	21
	call	helper
	end_function
"""),),
        expect=21,
        structure=("<- env.helper", "undefined explicit_name"),
        proves="an env function import, and that it survives as an import through the link",
    ),
    # How a static constructor is spelled, which upstream expressed as an
    # `(init_prio N)' decoration no assembler ever read, so constructors did not run
    # at all. The section kind has to be a bare `@': `@init_array', which is how this
    # directive reads on an ELF target, is rejected by the parser ("Expected eol,
    # instead got: init_array") -- the assembler takes the section's meaning from its
    # name alone. `init functions' in the dump is the proof that it did so, rather
    # than quietly emitting four bytes of data nothing would ever call.
    Subject(
        name="init_array",
        sources=(("init_array.s", """
	.text
	.globaltype	__stack_pointer, i32

	.hidden	setup
	.type	setup,@function
setup:
	.functype	setup () -> ()
	i32.const	flag
	i32.const	99
	i32.store	0
	end_function

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	flag
	i32.load	0
	end_function

	.type	flag,@object
	.section	.data.flag,"",@
	.globl	flag
	.p2align	2
flag:
	.int32	0
	.size	flag, 4

	.section	.init_array.101,"",@
	.p2align	2
	.int32	setup
"""),),
        expect=99,
        structure=("init functions", "priority=101 <setup>"),
        proves="an init_array section becoming an init function the linker collects",
    ),
    # How much of a NaN the dialect can spell, which `0005' answered from reading
    # rather than from measuring and got half wrong: it refused a *signed* NaN as
    # unspellable alongside a payload-carrying one, and the sign is accepted. That
    # mattered because `-quiet_NaN ()' in fast_float is what libstdc++ builds
    # `src/c++17/floating_from_chars.cc' from, so the refusal cost the target
    # std::from_chars for a float -- a `sorry' in the middle of a library build,
    # which is the loudest possible way for a wrong reading to present and still
    # took a full libstdc++ build to reach.
    #
    # The sign bit is extracted rather than the constant returned, because a
    # dialect that took `-nan' and quietly produced a positive NaN would satisfy
    # anything weaker: this is the one assertion that distinguishes "accepted" from
    # "accepted and meant it".
    Subject(
        name="nan_sign",
        sources=(("nan_sign.s", """
	.text
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	f64.const	-nan
	i64.reinterpret_f64
	i64.const	63
	i64.shr_u
	i32.wrap_i64
	end_function
"""),),
        expect=1,
        structure=("F <entry>",),
        proves="a signed NaN, kept negative through the assembler",
    ),
    # The other half of the same question, and the half `0005' had right: wat's
    # `nan:0x<payload>' is not in this dialect, so a NaN that is not canonical cannot
    # be emitted at all and the backend must `sorry' rather than print a bare `nan'
    # for it -- which would silently substitute a different constant. Red on purpose,
    # and the one place that fact is checked rather than asserted in a comment.
    Subject(
        name="nan_payload",
        sources=(("nan_payload.s", """
	.text
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (f64)
	f64.const	nan:0x8000000000000
	end_function
"""),),
        expect=0,
        structure=("F <entry>",),
        proves="nothing: a NaN payload cannot be spelled, so a non-canonical NaN must sorry",
        blocked="assemble failed",
    ),
    # Link-once semantics, which the target has none of: the backend defines
    # SUPPORTS_WEAK but no MAKE_DECL_ONE_ONLY, so SUPPORTS_ONE_ONLY is 0, so
    # __GXX_WEAK__ is 0, so libsupc++ needs an alias it cannot have. Whether a
    # one-only decl can be more than plain weak depends on this: whether the group
    # form of `.section' is accepted and survives as a comdat group the linker
    # deduplicates. Both objects define `dup' identically; one copy must reach the
    # module.
    #
    # The count in the structure assertion is the load-bearing part, and it cost an
    # hour: switching back to plain `.text' does NOT end the current group, so a
    # function emitted after a comdat one joins it, and the linker then discards
    # that function along with the duplicate group -- surfacing as `undefined
    # symbol' for a symbol plainly defined in the object. Every function therefore
    # needs its own explicit section, which is what the backend has to do too.
    Subject(
        name="comdat",
        sources=(("comdat_a.s", """
	.text
	.functype	part_b () -> (i32)

	.section	.text.dup,"G",@,dup,comdat
	.weak	dup
	.type	dup,@function
dup:
	.functype	dup () -> (i32)
	i32.const	11
	end_function

	.section	.text.entry,"",@
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	call	dup
	call	part_b
	i32.add
	end_function
"""), ("comdat_b.s", """
	.section	.text.dup,"G",@,dup,comdat
	.weak	dup
	.type	dup,@function
dup:
	.functype	dup () -> (i32)
	i32.const	11
	end_function

	.section	.text.part_b,"",@
	.globl	part_b
	.type	part_b,@function
part_b:
	.functype	part_b () -> (i32)
	i32.const	20
	call	dup
	i32.add
	end_function
""")),
        expect=42,
        structure=("comdat groups", "dup: [count=1]"),
        proves="comdat groups, so a one-only decl is deduplicated rather than only weak",
    ),
    # Whether `.set' can express an alias, which is what ASM_OUTPUT_DEF has to emit
    # and what `libsupc++/tinfo.cc' wanted when it failed with `alias definitions not
    # supported in this configuration'. Measured separately from the comdat subject on
    # purpose: one-only alone removes that particular alias, so a red here would cost
    # nothing immediately, and a green one is what makes ASM_OUTPUT_DEF worth wiring at
    # all. Both kinds are here because the two have different relocations behind them
    # and nothing says one implies the other.
    #
    # The structure assertion pins the aliases to the same function index and the same
    # segment offset as their targets, which is the whole claim -- a symbol that merely
    # exists would satisfy the link.
    Subject(
        name="alias",
        sources=(("alias_a.s", """
	.text
	.globl	real_fn
	.type	real_fn,@function
real_fn:
	.functype	real_fn (i32) -> (i32)
	local.get	0
	i32.const	3
	i32.mul
	end_function

	.globl	alias_fn
	.type	alias_fn,@function
	.functype	alias_fn (i32) -> (i32)
	.set	alias_fn, real_fn

	.type	real_obj,@object
	.section	.data.real_obj,"",@
	.globl	real_obj
	.p2align	2
real_obj:
	.int32	7
	.size	real_obj, 4

	.globl	alias_obj
	.set	alias_obj, real_obj
"""), ("alias_b.s", """
	.text
	.functype	alias_fn (i32) -> (i32)

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	alias_obj
	i32.load	0
	call	alias_fn
	end_function
""")),
        expect=21,
        structure=("F <real_fn> func=0", "F <alias_fn> func=0",
                   "D <real_obj> segment=0 offset=0", "D <alias_obj> segment=0 offset=0"),
        proves="`.set' aliases for a function and for a data symbol, resolving to their targets",
    ),
    # Whether a *reference* to a weak symbol that nothing defines can be left
    # unresolved, which is a different question from the weak definitions `comdat'
    # measures and the one libstdc++ turned out to be asking. `c++config' derives
    # _GLIBCXX_USE_WEAK_REF from __GXX_WEAK__, which `0007' set to 1, and reads it as
    # a promise that a function may be declared weak and never defined -- which is how
    # `cow-stdexcept.cc' refers to libitm's _ITM_* and _ZGTtnam without depending on
    # libitm. The target made that promise and had nothing behind it, so `_ITM_RU1'
    # and friends came out as ordinary undefined symbols and the link failed.
    #
    # The call is inside a branch a runtime zero never takes, because that is the
    # shape of the real thing: the reference has to be in the code for the question to
    # be asked, and executing it would measure wasm-ld's trapping stub instead. The
    # relocation in the structure assertion is what pins that down -- without it an
    # assembler that quietly dropped the call would return 42 just the same.
    Subject(
        name="weak_undef",
        sources=(("weak_undef.s", """
	.text
	.weak	maybe_absent
	.functype	maybe_absent () -> (i32)

	.section	.text.entry,"",@
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	0
	if	i32
	call	maybe_absent
	else
	i32.const	42
	end_if
	end_function
"""),),
        expect=42,
        structure=("undefined binding=weak", "R_WASM_FUNCTION_INDEX_LEB"),
        proves="a weak reference to a symbol nothing defines, left unresolved through the link",
    ),
    # Whether real exceptions are reachable from here at all, asked before anything
    # estimates them. The terminating unwinder of `0009' is honest about the target as
    # it stands and is not a step towards unwinding: with TARGET_EXCEPT_UNWIND_INFO at
    # UI_NONE there is nothing to walk, and the sjlj path cannot stand in for it either
    # -- it needs a setjmp a wasi-libc built without the exception-handling proposal
    # does not have. So the only real path is the proposal's own instructions, and the
    # first question about it is not GCC's: whether the tools downstream carry them.
    #
    # They do, all three of them -- a tag defined rather than imported, a throw, and a
    # try_table whose catch clause branches out with the payload -- so what stands
    # between here and `catch' working is entirely GCC-side: emitting EH regions at
    # all, which UI_NONE suppresses. That is a step, not a patch, and this is the row
    # that says it is not a dead end.
    #
    # A tag is defined by a plain label under its `.tagtype', which is the one piece
    # of this that had to be found by trying: `.globl' and `.tagtype' alone leave the
    # symbol undefined and wasm-ld refuses it, which is what the stock spelling of
    # `__cpp_exception' does too.
    Subject(
        name="eh_instructions",
        sources=(("eh_instructions.s", """
	.text
	.globl	my_tag
	.tagtype	my_tag i32
my_tag:

	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	block	i32
	try_table	(catch my_tag 0)
	i32.const	42
	throw	my_tag
	end_try_table
	i32.const	0
	end_block
	end_function
"""),),
        expect=42,
        structure=("E <my_tag> tag=0", "R_WASM_TAG_INDEX_LEB"),
        proves="a defined tag, `throw' and `try_table', assembled, linked and caught at runtime",
        # Required, and a finding rather than a detail: without it llvm-mc parses
        # try_table and then type-checks it as though the catch clause were not there
        # ("expected [i32] but got [i32, i32]"), so a backend emitting these would have
        # to name the flag in ASM_SPEC the way `0005' names the triple.
        asm_flags="-mattr=+exception-handling",
    ),
    # Step 19's three, and the first two are the spellings `0015' and `0016' have to
    # emit. They were not guessed: MSYS2's llvm package ships `llc', so the reference
    # text this script's header says was unavailable through rustc IS available for
    # these -- tiny LLVM IR through `llc -mtriple=wasm32-unknown-unknown
    # -mattr=+atomics,+bulk-memory --thread-model=posix' gives LLVM's own canonical
    # asm, and both subjects below are that output reduced to what they measure.
    #
    # The target_features custom section at the end of each is not decoration and is
    # the finding that shaped `threads': wasm-ld refuses thread-local storage with
    # "'bulk-memory' feature must be used in order to use thread-local storage" unless
    # the object declares both features, and -mattr alone does not write that section.
    # So a backend emitting TLS must emit the section too, and TLS and atomics cannot
    # be delivered one at a time -- which is what retired the scope's idea that `0015'
    # could land alone behind --disable-threads.
    Subject(
        name="tls",
        sources=(("tls.s", """
	.globaltype	__tls_base, i32

	.section	.text.entry,"",@
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	global.get	__tls_base
	i32.const	counter@TLSREL
	i32.add
	i32.load	0
	i32.const	35
	i32.add
	end_function

	.type	counter,@object
	.section	.tdata.counter,"T",@
	.p2align	2, 0x0
counter:
	.int32	7
	.size	counter, 4

	.section	.custom_section.target_features,"",@
	.int8	2
	.int8	43
	.int8	7
	.ascii	"atomics"
	.int8	43
	.int8	11
	.ascii	"bulk-memory"
"""),),
        expect=42,
        # `tls binding=local' is the half that says the symbol is thread-local storage
        # rather than ordinary data at the same address, which a green 42 alone would
        # not distinguish -- the single-threaded link places the TLS block at a fixed
        # offset, so a plain data symbol would return 42 as well.
        structure=("R_WASM_MEMORY_ADDR_TLS_SLEB", "tls binding=local",
                   "<env.__tls_base>", "[+] atomics", "[+] bulk-memory"),
        proves="a .tdata symbol addressed as an offset from __tls_base, relocated as TLS",
        asm_flags=f"-mattr={ASM_FEATURES}",
    ),
    # The atomic memory instructions, and the shared memory they are only useful in.
    # cmpxchg is here beside rmw.add because it is the one libstdc++ reaches for most --
    # every __atomic_compare_exchange_N the engine's lock-free queue compiles to ends
    # here -- and because `0016' has to emit a three-operand form, not just a
    # two-operand one.
    Subject(
        name="atomics",
        sources=(("atomics.s", """
	.section	.text.entry,"",@
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	0
	i32.const	35
	i32.atomic.rmw.add	shared
	drop
	atomic.fence
	i32.const	0
	i32.const	42
	i32.const	99
	i32.atomic.rmw.cmpxchg	shared
	end_function

	.type	shared,@object
	.section	.data.shared,"",@
	.p2align	2, 0x0
shared:
	.int32	7
	.size	shared, 4

	.section	.custom_section.target_features,"",@
	.int8	2
	.int8	43
	.int8	7
	.ascii	"atomics"
	.int8	43
	.int8	11
	.ascii	"bulk-memory"
"""),),
        # 7 + 35 under the atomic add, then a compare-exchange that expects exactly
        # that and so returns it. Distinguishable operands on purpose, which is step
        # 13's lesson from a __multi3 probe that passed under every permutation of its
        # arguments: a cmpxchg expecting the wrong value would also return 42 if the
        # add had not happened, so the expected value is the added one.
        expect=42,
        # MEMORY_ADDR_LEB, not SLEB: an atomic instruction's address is an unsigned
        # immediate, where a data-section pointer is signed. The first draft asserted
        # SLEB off the `static` subject's habit and read as a missing feature.
        #
        # The two feature lines are the corroboration that matters here, because
        # wasm-objdump -x lists no instruction names at all: a green 42 plus
        # MEMORY_ADDR_LEB would hold for a non-atomic load of the same address, while
        # the declared features are what the link refuses to proceed without.
        structure=("R_WASM_MEMORY_ADDR_LEB", "[+] atomics", "[+] bulk-memory"),
        proves="atomic rmw, fence and compare-exchange over a shared linear memory",
        asm_flags=f"-mattr={ASM_FEATURES}",
        threads=True,
    ),
    # The one subject here that is not about the dialect, kept here anyway because it
    # is the question that had to be answered before the reconfigure was paid for:
    # whether anything can HOST threads for this target. GCC is not involved -- the
    # answer is a property of wasi-libc, wasm-ld and node, and if it were red then
    # --enable-threads=posix would buy declarations that could never run.
    #
    # It links the real threads archives, so it doubles as the check on step 19's one
    # unverified assumption: that they need no symbol GCC cannot supply. They need
    # none at all -- the link has no undefined symbols beyond the WASI imports and
    # `wasi.thread-spawn', which the harness services with a Worker.
    #
    # The worker's side is wasi-libc's: pthread_create allocates the thread's stack and
    # descriptor in linear memory itself and the host only instantiates the same module
    # over the same memory, so `wasi_thread_start' is the whole contract.
    Subject(
        name="thread_spawn",
        sources=(("thread_spawn.s", """
	.functype	pthread_create (i32, i32, i32, i32) -> (i32)
	.functype	pthread_join (i32, i32) -> (i32)

	.section	.text.worker,"",@
	.type	worker,@function
worker:
	.functype	worker (i32) -> (i32)
	i32.const	0
	i32.const	35
	i32.atomic.rmw.add	acc
	drop
	i32.const	0
	end_function

	.section	.text.entry,"",@
	.globl	entry
	.type	entry,@function
entry:
	.functype	entry () -> (i32)
	i32.const	tid
	i32.const	0
	i32.const	worker
	i32.const	0
	call	pthread_create
	drop
	i32.const	0
	i32.load	tid
	i32.const	0
	call	pthread_join
	drop
	i32.const	0
	i32.atomic.load	acc
	end_function

	.type	tid,@object
	.section	.bss.tid,"",@
	.p2align	2, 0x0
tid:
	.int32	0
	.size	tid, 4

	.type	acc,@object
	.section	.data.acc,"",@
	.p2align	2, 0x0
acc:
	.int32	7
	.size	acc, 4

	.no_dead_strip	__indirect_function_table
	.section	.custom_section.target_features,"",@
	.int8	2
	.int8	43
	.int8	7
	.ascii	"atomics"
	.int8	43
	.int8	11
	.ascii	"bulk-memory"
"""),),
        # 42 only if the spawned thread really ran: the main thread adds nothing, and
        # it reads `acc' after pthread_join rather than after a sleep, so a host that
        # returned a plausible tid without starting anything returns 7.
        expect=42,
        structure=("R_WASM_TABLE_INDEX_SLEB", "R_WASM_MEMORY_ADDR_SLEB"),
        proves="pthread_create and pthread_join against the threads sysroot, joined and observed",
        asm_flags=f"-mattr={ASM_FEATURES}",
        threads=True,
        sysroot_libs=("c",),
    ),
)


def threads_sysroot(msys_root: Path) -> Path:
    """The threads target's own library directory, inside the unflattened tarball.

    Reusing build_gcc_wasm's staging rather than its flattened sysroot, and the
    distinction is the whole value of the `thread_spawn` subject: it links against the
    archives the GCC cross is being reconfigured onto, so a symbol they need and
    nothing supplies is a measurement. A flattened copy would also work right up until
    this script were run before the other one, which is exactly the order step 19 uses.

    The default workdir and cache are build_gcc_wasm's own defaults rather than this
    script's, because the tarball belongs to that script and downloading a second copy
    under .wasm-dialect would be a second thing to keep pinned.
    """
    staged = stage_sysroot(msys_root, GENERATED_ROOT / ".gcc-wasm-build",
                           GENERATED_ROOT / ".gcc-src-cache")
    return staged / "lib" / WASI_TARGET


class Result(NamedTuple):
    status: str
    detail: str


def write_sources(workdir: Path, subject: Subject) -> list:
    stems = []
    for filename, text in subject.sources:
        with open(workdir / filename, "w", encoding="utf-8", newline="\n") as f:
            f.write(text.lstrip())
        stems.append(Path(filename).stem)
    return stems


def missing_structure(workdir: Path, stem: str, wanted: tuple) -> list:
    """Which required object facts `wasm-objdump -x` did not show.

    The dump goes through a file rather than a captured pipe because msys2.bash
    deliberately does not capture output -- it streams, so a long GCC build stays
    watchable. Redirecting here keeps that shared helper unchanged.
    """
    dump = (workdir / f"{stem}.dump").read_text(encoding="utf-8", errors="replace")
    return [want for want in wanted if want not in dump]


def measure(msys_root: Path, workdir: Path, subject: Subject, gc_sections: bool) -> Result:
    """Take one subject from text to a number, naming the stage it stopped at.

    Stages are named because this script's whole value is in telling apart "LLVM's
    dialect cannot say this" from "my hand-written asm is wrong", and those two fail
    at different stages.
    """
    stems = write_sources(workdir, subject)
    cd = f"cd {to_msys(workdir)} && "
    for stem in stems:
        if bash(msys_root, cd + f"llvm-mc -triple={TRIPLE} {subject.asm_flags} "
                                f"-filetype=obj {stem}.s -o {stem}.o",
                check=False).returncode != 0:
            return Result("assemble failed", f"llvm-mc rejected {stem}.s")

    if bash(msys_root, cd + f"wasm-objdump -x {stems[0]}.o > {stems[0]}.dump",
            check=False).returncode != 0:
        return Result("dump failed", f"wasm-objdump could not read {stems[0]}.o")
    absent = missing_structure(workdir, stems[0], subject.structure)
    if absent:
        return Result("structure missing", ", ".join(absent))

    objects = " ".join(f"{stem}.o" for stem in stems)
    # gc-sections is left ON by default, which the retired rwat pipeline could not do:
    # GCC's unsymbolled __stack_pointer import was collected away and every module died
    # on `global.get 0`. llvm-mc gives that global a real symbol, so the default being
    # the default here is what keeps measuring that rather than assuming it. The flag
    # stays available to tell a collection failure apart from a relocation one.
    flag = "" if gc_sections else " --no-gc-sections"
    # The constructor entry point is exported where the linker synthesised one, which
    # it does only for a subject that contributed an init-array entry -- hence
    # if-defined rather than a plain export, which would fail the other five.
    #
    # A threads subject links against the threads sysroot the GCC cross is being
    # reconfigured onto, not against a copy of its headers: the whole point of
    # `thread_spawn` is that these are the real archives, so a symbol they want and
    # nothing supplies is a result rather than something the subject could hide.
    # --export-memory is threads-only and was found by measurement -- node:wasi binds
    # to `instance.exports.memory` and refuses a module that merely imports one.
    #
    # The reactor startfile goes with the libc and not with `threads`: `atomics` wants
    # a shared memory and nothing else, and handing it crt1-reactor.o without a -lc
    # behind it fails the link on the crt's own undefined symbols -- which is how this
    # separation got found.
    pre, post = "", ""
    if subject.sysroot_libs:
        libs = threads_sysroot(msys_root)
        pre = f"{to_msys(libs / 'crt1-reactor.o')} "
        post = (f" -L{to_msys(libs)} "
                + " ".join(f"-l{lib}" for lib in subject.sysroot_libs)
                + " --export-if-defined=_initialize")
    if subject.threads:
        post += (" --shared-memory --import-memory --export-memory"
                 f" --max-memory={HARNESS_MAX_PAGES * PAGE_SIZE}")
    if bash(msys_root, cd + f"wasm-ld {pre}{objects} -o {subject.name}.wasm --no-entry "
                            f"--export={ENTRY} "
                            f"--export-if-defined=__wasm_call_ctors{flag}{post}",
            check=False).returncode != 0:
        return Result("link failed", "wasm-ld rejected the objects")

    mode = " shared" if subject.threads else ""
    code = bash(msys_root, cd + f"node --no-warnings harness.mjs {subject.name}.wasm "
                                f"{subject.expect}{mode}",
                check=False).returncode
    if code == NO_INSTANCE_EXIT:
        return Result("did not instantiate", "the linked module failed validation")
    if code == TRAP_EXIT:
        return Result("trapped", "a relocation resolved to an invalid index")
    if code != 0:
        return Result("wrong result", f"expected {subject.expect}")
    return Result("ran", f"{subject.expect}")


def main() -> None:
    p = argparse.ArgumentParser(
        description="Measure LLVM's wasm assembly dialect against what GCC's backend needs")
    p.add_argument("--msys-root", type=Path, default=DEFAULT_MSYS_ROOT)
    p.add_argument("--workdir", type=Path, default=GENERATED_ROOT / ".wasm-dialect")
    p.add_argument("--no-gc-sections", action="store_true",
                   help="Link with --no-gc-sections, to tell a collection failure apart "
                        "from a relocation one")
    p.add_argument("--skip-install", action="store_true",
                   help="Assume the tools are present instead of running pacman")
    args = p.parse_args()

    msys_root = args.msys_root.resolve()
    workdir = args.workdir.resolve()
    workdir.mkdir(parents=True, exist_ok=True)

    if not args.skip_install:
        print("\n== tools ==")
        bash(msys_root, f"pacman -S --noconfirm --needed {' '.join(TOOLS)}", msystem="MSYS")

    with open(workdir / "harness.mjs", "w", encoding="utf-8", newline="\n") as f:
        f.write(harness_text())

    print("\n== subjects ==")
    results = [(subject, measure(msys_root, workdir, subject, not args.no_gc_sections))
               for subject in SUBJECTS]

    print("\n== result ==")
    # A blocked subject that starts passing is news, not a failure, and it is the one
    # case that must not be reported as "the dialect did not carry this" -- it carries
    # it now. Same split, for the same reason, as build_gcc_wasm's result loop.
    failed = []
    now_passing = []
    for subject, result in results:
        if subject.blocked and result.status == "ran":
            mark = "NOW PASSES - drop its `blocked`"
            now_passing.append(subject.name)
        elif subject.blocked and result.status == subject.blocked:
            mark = f"expected red: {result.status}"
        elif result.status == "ran":
            mark = "PROVEN"
        else:
            mark = f"FAILED: {result.status}"
            failed.append(subject.name)
        print(f"  {subject.name:15} {mark:30} {result.detail}")
        print(f"  {'':15} proves: {subject.proves}")

    print(f"\nWorkdir: {workdir}")
    if failed:
        raise SystemExit(
            f"the LLVM dialect did not carry: {', '.join(failed)}. Per the verdict rule in "
            "docs/wasm_pipeline_plan.md this counts against porting GCC onto it, but check "
            "the named stage first -- `assemble failed` is more likely a flaw in the "
            "hand-written subject than a gap in the dialect.")
    if now_passing:
        raise SystemExit(
            f"the LLVM dialect now carries: {', '.join(now_passing)}. Drop the `blocked` "
            "field, and check what in GCC's output was built around the gap -- "
            "`late_functype` passing would mean the module header is no longer needed.")
    print("Every subject reached the status it is expected to. LLVM's dialect carries data\n"
          "symbols, TABLE_INDEX relocations in data and local binding -- the three things\n"
          "rwat lacks -- plus a result-typed dispatch loop, branch-by-depth, env imports,\n"
          "an init_array section the linker turns into __wasm_call_ctors, comdat groups,\n"
          "`.set' aliases, weak references to symbols nothing defines, and -- behind\n"
          "-mattr=+exception-handling -- the exception-handling proposal's own tags,\n"
          "`throw' and `try_table'. Behind -mattr=+atomics,+bulk-memory it carries\n"
          "thread-local storage as an offset from __tls_base and the atomic memory\n"
          "instructions, and real pthreads link and run against the threads sysroot.\n"
          "It does not let a signature be declared after its call, which is why the module\n"
          "header exists, and `.text' does not end a comdat group, which is why every\n"
          "function needs its own section. It does not let TLS and atomics be taken one at\n"
          "a time: wasm-ld refuses thread-local storage unless the object declares both\n"
          "features, so a backend emitting either must emit the target_features section.")


if __name__ == "__main__":
    main()
