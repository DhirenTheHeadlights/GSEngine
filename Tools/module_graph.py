"""The engine's module graph, derived rather than written down.

One question, asked by `build_gcc_wasm.py --sweep`: which of the engine's modules can be
translated for a target that has no Windows, and where exactly does the host frontier
sit. Answering it needs the interface units in dependency order, and the order is a fact
about 425 module units that nobody should be maintaining by hand -- build_gcc_wasm.py
used to carry four such lists, and the one for gse.math was nineteen entries deep.

TWO SOURCES, JOINED ON THE LOGICAL NAME, because each is authoritative about one half:

  * the `module gse.x:part;` declaration is read off disk. It cannot sit inside a
    #ifdef -- it is the first thing after the global module fragment, which is as deep
    as line 34 in `Source/External/DirectX.cppm` -- so a regex is exact here, and
    reading the files is the only way to learn a *path*: the scan names logical modules
    and object files, never a source.

  * the requires-edges come from CMake's own dependency scan -- the `.ddi` files under
    the native build directory, which is the compiler's answer rather than a guess at
    one. This matters: an `import` line CAN sit inside a #ifdef, and the Vulkan/DX12
    backend selection is exactly that shape, so a line-based scan would report a module
    as reaching both backends when the build only ever takes one. `.ddi` has already
    run the preprocessor.

The join is also the staleness check. Every interface on disk must appear in the scan
and every scanned logical name must have a file; either direction failing means the
build directory predates the tree, and `stale_interfaces`/`unresolved` say so rather
than letting a sweep measure a graph the tree no longer has.

`analyze_module_graph.py` reads the same `.ddi` files for the build-time critical path
and is where the format was already proven readable.

Standalone, which is how the derivation gets checked without a cross compiler in the
loop:

    python Tools/module_graph.py                 # the module table and the host frontier
    python Tools/module_graph.py gse.math        # one closure, in compile order
    python Tools/module_graph.py --impls gse     # and the impl units a link of it needs
"""
import collections
import json
import re
import sys
from pathlib import Path, PurePosixPath
from typing import NamedTuple

from msys2 import REPO_ROOT

# Where module units live, interface and implementation alike -- both walks below read
# this one tuple. Import/ carries the primary interface of almost every module and
# Source/ carries the partitions, so neither alone is a target.
#
# The Sandbox is here because the scenario is. `scene_reset_cpu` is declared in
# `sandbox:scenarios` and nothing under Engine/ provides it, so a link that stopped at
# the engine could compile every unit the browser port needs and still have nothing to
# run; `LINK_SUBJECT`'s own comment in build_gcc_wasm.py recorded that as the reason the
# engine went first and the Sandbox second. Adding the two directories is the whole of
# the second half: the walks key on a module declaration and a suffix, neither of which
# knows which tree it is reading, and CMake does not read this file -- the four data
# files it does share with these walks name units, not roots, and are untouched.
# `Engine/Server` is the third, and the first link of the Sandbox is what named it:
# `gse.server` lives there, in two interface units with no implementation unit at all, and
# `Sandbox/Source/NetSetup.cpp` is its only importer in the tree. A root that stopped at
# Engine/Engine therefore left that module with no BMI and rejected the one unit defining
# `server_setup`, which `Main.cpp` needs whether or not a headless scenario ever takes the
# dedicated-server branch.
INTERFACE_ROOTS = ("Engine/Engine/Source", "Engine/Engine/Import",
                   "Engine/Server",
                   "Sandbox/Sandbox/Source", "Sandbox/Sandbox/Import")

# The host frontier's roots, and they are a structural fact about the tree rather than a
# judgment about portability: every module under Source/External is a wrapper over a
# platform or vendored C library -- gse.win32, gse.directx, gse.freetype, gse.moodycamel
# and eleven more -- and there is no such thing as porting one. Everything else that is
# host-bound is host-bound *because its closure reaches one of these*, which is derived
# below and printed as a chain. Nothing is excused by being named in a list here.
HOST_DIR = "Engine/Engine/Source/External"

# The exception the sentence above did not allow for, and it is not a judgment either.
# "There is no such thing as porting one" is true of a wrapper over a platform API and
# false of a wrapper over a *portable* vendored library: `Moodycamel.cppm` is twelve lines
# over a header whose Windows arm (concurrentqueue.h's `GetCurrentThreadId`) is one of
# four, and the arm wasm32 selects is a thread_local address. A module named here is
# treated as an ordinary unit -- offered to the compiler, and its verdict propagated to
# its dependents like any other -- rather than short-circuited into a host-bound row.
#
# This is the one list in this file that *is* a claim, so it is kept honest by what it
# does NOT do: it does not excuse the unit, it exposes it. Membership means the sweep
# compiles the file every run, so the row is a measurement that can go red; a wrapper
# named here that stops translating reports `failed`, which is news, instead of
# `host-bound`, which is arithmetic. Nothing belongs here until a sweep has shown it
# green.
#
# The test an #ifdef-armed wrapper has to pass, since two of the three here are one.
# `gse.win32` would fail it and is the reason the test is written down: its non-Windows
# arm is EMPTY, so every caller still names `win32::` symbols that are not there, and
# listing it would move the failure from one host-bound row to a hundred rejections --
# the OS seam's work dressed up as a measurement trick. `gse.winhttp`'s non-Windows arm
# is the other thing entirely: it carries the whole surface its one caller names
# (`handle`, `supported`, every `error_*` constant), and `Http/Client.cpp` already has
# the `#else` body that uses it. So the question is not whether the wrapper has an
# #ifdef, it is whether the neutral arm is COMPLETE for the callers there are -- and a
# wrapper that is wrong about that answers with a rejection, here, every run.
PORTED_HOST_ROOTS = ("gse.moodycamel", "gse.winhttp")

# Units a target with no native GPU backend does not have, interface and impl alike, and
# the one thing in this file that is not derived from the tree -- so it is not written here
# either. CMake drops the same list from the engine target
# (`gse_drop_native_gpu_backend_units`), and the file explains why both sides read it
# rather than each testing a directory. Both walks below read it, since which of them a
# listed unit falls in is a property of its suffix and not of the claim.
#
# Not a portability judgment, and not the same kind of claim as PORTED_HOST_ROOTS above.
# A unit named here is one the wasm link would not contain, so measuring it would report
# the absence of a backend that configuration never asked for; a unit named there is one
# the sweep compiles every run.
NATIVE_GPU_BACKEND_LIST = "Engine/cmake/native_gpu_backend_units.txt"

# The other three data files CMake and this walk both read, on the other half of the
# engine: its implementation units. The two arm lists carry directory NAMES and the
# exclusion list carries paths, so they are read by the two different helpers below --
# see each file for why it is a file.
#
# The policy those arm names are selected BY is not shared and is stated here: the wasm
# link keeps the Web arm of every platform choice and the Null arm of the profiling
# backend, because it has no Windows and no Nsight Perf SDK. CMake's own sentence is the
# other one -- Win32 plus Native, and the backend find_package found -- and the two are
# not a common datum dressed up as a difference.
PLATFORM_ARMS_LIST = "Engine/cmake/platform_impl_arms.txt"
BACKEND_ARMS_LIST = "Engine/cmake/gpu_profile_backend_arms.txt"
EXCLUDED_IMPLS_LIST = "Engine/cmake/excluded_impl_units.txt"
WASM_PLATFORM_ARM = "Web"
WASM_BACKEND_ARM = "Null"

# Where CMake leaves the dependency scan. One configuration directory under it, and the
# sweep names the one it read: merging two would union the #ifdef arms that reading
# `.ddi` instead of the source exists to resolve.
DEFAULT_BUILD = "out/build"

# Dropped from every unit's edges: every unit imports `std`, and a dependency they all
# share orders nothing. Named exactly rather than matched by prefix, so that an engine
# module whose name begins with these letters could never be dropped silently.
STANDARD_MODULES = ("std", "std.compat")

MODULE_DECL = re.compile(
    r"^\s*(export\s+)?module\s+([A-Za-z_][\w.]*)\s*(?::\s*(\w+))?\s*;")

# The line is `.cppm` against `.cpp`, NOT `export module` against `module`. An
# unexported partition in a .cppm -- `module gse.runtime:attached_link;`, and there are
# four of them -- is an implementation partition, and it still produces a BMI that
# `gse.runtime:bootstrap` imports, so a graph that dropped it would report three of
# bootstrap's edges as unresolvable. What this measurement leaves out is the `.cpp`
# implementation units, where the host lives: `Import/Assert.cpp` imports gse.win32 and
# starts a thread, which is a host port rather than a translation question.
SOURCE_SUFFIX = "*.cppm"

# And the implementation units the paragraph above says this measurement leaves out. It
# leaves them out of the SWEEP, which is a question about translation and is answered one
# interface unit at a time; a LINK is the other question, and there is no such thing as
# linking the engine without them. So they are enumerated here too, separately, and no
# sweep reads them: `impls_of` is what the wasm link's unit list is built from, and the
# sweep order is still interfaces alone.
IMPL_SUFFIX = "*.cpp"


def module_of(logical: str) -> str:
    """The module a logical name belongs to: `gse.math:units` is gse.math's."""
    return logical.split(":", 1)[0]


def wraps_host(logical: str, path: str) -> bool:
    """Whether this unit is a host root: a wrapper, and not one measured as ported.

    One function because there were two copies of the directory test -- `Graph.is_host_root`
    and the `is_root` set `scan` builds -- and they answer the same question for the two
    halves of the same verdict. Two copies of it is how a ported wrapper could be compiled
    by the sweep while still propagating host-boundness to everything that imports it,
    which is the one outcome worse than either answer alone.
    """
    return path.startswith(HOST_DIR) and module_of(logical) not in PORTED_HOST_ROOTS


def listing(repo_root: Path, name: str) -> tuple:
    """One of the shared data files, as its meaningful lines.

    What a line means has to be the same here as in CMake's `_gse_read_listing`, because
    these files exist to be read by both: a blank or a # comment is not an entry, and the
    whitespace around one is not part of it.
    """
    return tuple(entry for entry in
                 (line.strip() for line in
                  (repo_root / name).read_text(encoding="utf-8").splitlines())
                 if entry and not entry.startswith("#"))


def path_listing(repo_root: Path, name: str) -> frozenset:
    """One of them whose lines are repo-relative paths, as posix paths.

    A listed path that does not exist raises. The list is the one declaration two
    enumerators read, so a renamed unit that silently stopped being excluded would leave
    CMake and this walk disagreeing about what a build contains -- which is the failure
    the single declaration exists to prevent, arriving by the back door.
    """
    paths = listing(repo_root, name)
    for rel in paths:
        if not (repo_root / rel).exists():
            raise SystemExit(f"{name} names {rel}, which does not exist; the list is "
                             f"read by CMake too and has to name the unit's current path")
    return frozenset(paths)


def kept_arm(rel: str, arms: tuple, selected: str) -> bool:
    """Whether this impl unit survives one selection axis.

    The same test as CMake's two `gse_select_*_impls`: a `.cpp` directly under an arm
    directory belongs to that arm, and every arm but the selected one is dropped. A file
    under no arm directory at all is under no choice and is kept by every configuration --
    most of the engine's impl units are, which is why this asks whether a DECLARED arm
    names the parent rather than whether the parent looks like a platform.
    """
    parent = PurePosixPath(rel).parent.name
    return parent not in arms or parent == selected


class Graph(NamedTuple):
    """Interface units by logical name, their edges, and what did not join up.

    `requires` is per logical name and carries only names this engine declares -- `std`
    and the C++ standard library's partitions are dropped, because every unit needs them
    and a dependency everything shares orders nothing.
    """
    build: Path
    path_of: dict
    requires: dict
    units_by_module: dict
    # Module -> every host root its imports reach, which is a set and not a nearest
    # one. See `host_roots_of`.
    roots_of: dict
    unexported: tuple
    # Module -> its implementation units for a wasm configuration, and the `.cpp` files
    # that declared no module. Not nodes in the graph and not in any sweep: see
    # `scan_impls` and `impls_of`.
    impls: dict
    undeclared_impls: tuple
    # A `.cppm` carrying no module declaration at all. Not a node in the graph, and
    # reported rather than dropped -- see `scan_interfaces`.
    undeclared: tuple
    # An interface on disk that the dependency scan has never seen, which means the
    # build directory is older than the file.
    stale_interfaces: tuple
    # A scanned edge naming something no interface on disk provides. Real, and not an
    # error: a module whose only interface is generated, or a name the scan carries from
    # a target this sweep does not read.
    unresolved: dict

    def modules(self) -> list:
        """Every module with at least one interface unit, in name order."""
        return sorted(self.units_by_module)

    def units_of(self, module: str) -> list:
        """The module's own interface units: its primary interface and its partitions."""
        return self.units_by_module.get(module, ())

    def imports_of(self, module: str) -> set:
        """The other modules this one's interface units name."""
        return {module_of(req)
                for logical in self.units_of(module)
                for req in self.requires.get(logical, ())
                if module_of(req) != module}

    def external_requires(self, logical: str) -> tuple:
        """What this unit imports that no file in the engine provides.

        `gse.vulkan:types` imports `vulkan`, the SDK's own module, and nothing under
        Engine declares it. That is not a target gap and must not be compiled as though
        it might be: the unit would fail on a missing module and the row would read
        FAILED, which outranks host-bound and would have reported the Vulkan SDK's
        absence as a wasm32 finding.
        """
        return tuple(req for req in self.requires.get(logical, ())
                     if req not in self.path_of)

    def order(self, roots: list) -> tuple:
        """`roots` and everything they reach, dependencies first.

        Post-order depth-first, which is a topological order whenever the graph is
        acyclic -- and an import cycle between interfaces is ill-formed C++, so one here
        would be a bug in the scan rather than a shape to support. It is detected anyway
        and raised, because the alternative is recursing until the stack goes.
        """
        seen, on_stack, out = set(), [], []

        def visit(logical: str) -> None:
            if logical in seen:
                return
            if logical in on_stack:
                cycle = " -> ".join(on_stack[on_stack.index(logical):] + [logical])
                raise SystemExit(f"import cycle among interfaces: {cycle}")
            on_stack.append(logical)
            for req in self.requires.get(logical, ()):
                if req in self.path_of:
                    visit(req)
            on_stack.pop()
            seen.add(logical)
            out.append(logical)

        for root in roots:
            visit(root)
        return tuple(out)

    def paths(self, roots: list) -> tuple:
        """`roots` and everything they reach, as repo-relative paths in compile order."""
        return tuple(self.path_of[logical] for logical in self.order(roots))

    def partitions_of(self, module: str) -> list:
        """The module's partitions, without its primary interface."""
        return [logical for logical in self.units_of(module) if ":" in logical]

    def closure(self, module: str) -> tuple:
        """Every interface unit needed to compile MODULE, as repo-relative paths.

        This is what replaced build_gcc_wasm.py's hand-written source lists. The roots
        are all of the module's own units rather than only its primary interface: a
        partition the primary does not re-export -- `gse.ecs:system_manifest` is one --
        is still part of the module and still has to compile.

        An unknown module raises instead of returning nothing. The hand-written lists
        could not be misspelled without the file failing to open; a module name can, and
        an empty closure makes a probe compile zero files and report COMPILED -- a green
        row measuring nothing, which is the one outcome a probe table must never
        produce.
        """
        if module not in self.units_by_module:
            raise SystemExit(f"no module named {module} in the engine's graph; "
                             f"{len(self.units_by_module)} modules were scanned")
        return self.paths(self.units_of(module))

    def link_closure(self, module: str) -> tuple:
        """Every interface unit a LINK of MODULE needs, as repo-relative paths.

        Wider than `closure`, and the first engine link is what proved it has to be.
        `closure` follows import edges, so for a dependency module it brings in the
        partitions something in the closure imports and no others -- which is exactly
        right for a translation question, because an interface nothing imports cannot
        affect whether another interface compiles.

        It is wrong for a link. An implementation unit of gse.gpu imports
        `gse.gpu:command_dispatch` whether or not any interface does, and `.cpp` files
        contribute no edges to this graph at all: `scan_requires` reads `*.cppm.ddi`, and
        `scan_impls` reads a module declaration and nothing else. So the partition was
        absent from the unit list, its BMI was whatever a previous run had left in the
        shared module cache, and sixteen impl units were rejected with `import 'std' has
        CRC mismatch` -- a measurement artifact of the driver, reported as though the
        engine had sixteen more portability gaps than it has.

        Hence: every unit of every module the closure reaches, to a fixpoint, since a
        partition pulled in this way may name a module no other unit did. A module's own
        units are already all of `closure`'s roots; this says the same of its
        dependencies.
        """
        return self.paths(self.link_roots(module))

    def link_roots(self, module: str) -> tuple:
        """The unit list `link_closure` and `impls_of` are both derived from.

        One derivation and not two, which is the whole reason this is its own function:
        the interfaces a link compiles and the bodies it compiles are halves of one unit
        list, and a module present in one half and absent from the other is precisely the
        artifact `link_closure` exists to remove -- interfaces compiled with no bodies
        behind them, reported as undefined symbols. Each half computing the set for
        itself would agree today and only one of them would be updated.

        The fixpoint is over modules because a partition pulled in by the widening may
        name a module no other unit did. It adds no module to this engine as it stands --
        all nine units it adds are partitions of modules already reached -- so the loop
        runs twice and stops; it is here for the case that makes the two halves differ,
        which is the case that would otherwise be silent.

        Every PORTED_HOST_ROOT is a root here too, because the fixpoint cannot reach one
        that only an implementation unit imports and `gse.winhttp` is exactly that: the
        docstring above says `.cpp` files contribute no edges to this graph, and
        `Http/Client.cpp` is the sole importer in the tree. Without this the wrapper is in
        neither half -- not short-circuited as host-bound, since it is named as ported,
        and not offered either -- so its one consumer is rejected for a BMI nobody was
        ever going to build. Naming them unconditionally also keeps membership in that
        list meaning what it says it means: the file is compiled every run, so the row is
        a measurement rather than an argument.
        """
        reached = {module_of(logical)
                   for logical in self.order(self.units_of(module))}
        reached |= {root for root in PORTED_HOST_ROOTS
                    if root in self.units_by_module}
        while True:
            roots = tuple(logical for m in sorted(reached)
                          for logical in self.units_of(m))
            grown = {module_of(logical) for logical in self.order(roots)}
            if grown == reached:
                return roots
            reached = grown

    def impls_of(self, module: str) -> tuple:
        """Every implementation unit needed to LINK MODULE, as repo-relative paths.

        `closure` is the other half and comes first: these are compiled after every
        interface unit, because a plain implementation unit of gse.x can only be compiled
        once gse.x has a BMI, and the whole closure has to exist for that to be true of
        all of them at once.

        Over `link_roots` and not `units_of`, so that the bodies cover exactly the
        modules whose interfaces `link_closure` compiles. See that function: a module in
        one half and not the other is an artifact, not a measurement.

        Ordered by their module's position in that list, which is topological, and by path
        within a module. Nothing in this tree needs that order -- no unit imports an
        implementation partition, so there are no edges among these at all -- but an order
        derived from the one that is already correct costs nothing and is the one that
        stays correct if something does.

        Repeats `closure`'s refusal on an unknown module for its reason: an empty list
        makes a link compile zero bodies and report every symbol undefined, which is a red
        measuring nothing where `closure`'s was a green measuring nothing.
        """
        if module not in self.units_by_module:
            raise SystemExit(f"no module named {module} in the engine's graph; "
                             f"{len(self.units_by_module)} modules were scanned")
        ordered, seen = [], set()
        for logical in self.order(self.link_roots(module)):
            owner = module_of(logical)
            if owner in seen:
                continue
            seen.add(owner)
            ordered.extend(sorted(self.impls.get(owner, ())))
        return tuple(ordered)

    def sweep_order(self) -> tuple:
        """Every interface unit in the engine, dependencies first."""
        return self.order(sorted(self.path_of))

    def is_host_root(self, logical: str) -> bool:
        return wraps_host(logical, self.path_of[logical])

    def host_chain(self, module: str) -> list:
        """The shortest import chain from MODULE to a host root, or [] if there is none.

        Breadth-first so the chain that gets printed is the shortest one, which is the
        one a reader can act on: `gse.time -> gse.win32` says what to do, while a walk
        that wandered through six modules first does not.
        """
        if any(self.is_host_root(logical) for logical in self.units_of(module)):
            return [module]
        queue = collections.deque([[module]])
        seen = {module}
        while queue:
            chain = queue.popleft()
            for nxt in sorted(self.imports_of(chain[-1])):
                if nxt in seen or nxt not in self.units_by_module:
                    continue
                if any(self.is_host_root(logical) for logical in self.units_of(nxt)):
                    return chain + [nxt]
                seen.add(nxt)
                queue.append(chain + [nxt])
        return []

    def host_roots_of(self, module: str) -> frozenset:
        """EVERY host root MODULE's imports reach, not the nearest one.

        A module typically sits behind several independent wrappers -- `gse.gpu` reaches
        gse.win32 through `Gpu/Context.cppm`'s gse.os, gse.nsight_perf through
        `Graph/RenderGraph.cppm`, and gse.vulkan and gse.dx12 through its two device
        backends -- and a shortest-path answer names one of the four. That is the
        difference between "porting this makes 21 units measurable" and "porting this
        makes none of them measurable", so the full set is the only honest one.

        `roots_of` is computed once for the whole graph in `scan`, by fixpoint rather
        than by recursion, because the module-level import graph may contain cycles that
        the unit-level one cannot.
        """
        return self.roots_of.get(module, frozenset())

    def frontier(self) -> list:
        """Host roots, each with what it gates: (root, reached-by, sole-root-for).

        This is what the sweep is really for. A dependent cannot be offered to the
        compiler at all while a module it imports is host-bound -- there is no BMI for
        it -- so the propagation is the target's doing rather than a choice, and the
        count of host-bound modules is not a measure of the engine's portability.

        TWO COLUMNS, because the one that reads like the answer is not. `reached` counts
        every module whose imports arrive here, and those sets overlap heavily -- the
        same module is in four of these rows. `sole` counts the modules this root is the
        *only* wrapper in front of, which is the number the question "what does porting
        this one unblock" actually asks: everything else needs its other roots ported
        too, and a row with `reached` high and `sole` zero unblocks nothing on its own.
        Reporting only the first is how step 18's draft came to claim that gse.nsight_perf
        was the only thing in front of gse.gpu's 21 units.
        """
        rows = []
        for root in sorted({r for roots in self.roots_of.values() for r in roots}):
            # The root is behind itself, trivially, and counting that is how a wrapper
            # that unblocks nothing still reports a 1.
            reached = [m for m, roots in sorted(self.roots_of.items())
                       if root in roots and m != root]
            sole = [m for m in reached if self.roots_of[m] == frozenset({root})]
            rows.append((root, reached, sole))
        return sorted(rows, key=lambda row: (-len(row[2]), -len(row[1]), row[0]))

    def gated_groups(self) -> list:
        """Modules grouped by the exact set of roots in front of them, largest first.

        The frontier's per-root columns say what each wrapper is involved in; this says
        what would have to be done together, which for this engine is the only question
        with a useful answer. No single wrapper is the sole root of anything, so a
        per-root reading of "port this and N become measurable" is always zero -- the
        sets are what move.
        """
        groups = collections.defaultdict(list)
        for module, roots in sorted(self.roots_of.items()):
            if roots and roots != frozenset({module}):
                groups[roots].append(module)
        return sorted(groups.items(), key=lambda row: (-len(row[1]), sorted(row[0])))


def host_roots(units_by_module: dict, imports: dict, is_root: set) -> dict:
    """Module -> every host root it reaches, by fixpoint.

    A fixpoint and not a depth-first walk: the unit-level graph is acyclic because an
    import cycle between interfaces is ill-formed, but the *module*-level one has no
    such guarantee -- a partition of A may import B while another partition of B imports
    A, which is well-formed and which a recursive walk would either loop on or have to
    special-case. Iterating until nothing changes costs a handful of passes over 65
    modules and needs neither.
    """
    roots = {module: frozenset({module} if module in is_root else ())
             for module in units_by_module}
    changed = True
    while changed:
        changed = False
        for module, reachable in roots.items():
            grown = reachable.union(*(roots.get(dep, frozenset())
                                      for dep in imports[module])) \
                if imports[module] else reachable
            if grown != reachable:
                roots[module] = grown
                changed = True
    return roots


def scan_interfaces(repo_root: Path) -> tuple:
    """Logical name -> repo-relative path, off disk; the unexported partitions; the
    files that declared no module at all.

    The declaration is not always the first line -- `Source/External/DirectX.cppm`
    reaches line 34, behind a global module fragment of #includes -- so the whole file
    is read rather than a guessed-at head. A `.cppm` with no declaration in it is
    returned as `undeclared` rather than skipped: dropping one silently removes a node
    from the graph, and every edge into it then reappears as an unresolved import or a
    host-bound verdict, which is a loud symptom with its cause erased.

    `module;` -- the global fragment -- and `module : private;` both fail the name
    pattern, so neither is mistaken for a declaration.
    """
    path_of, unexported, undeclared = {}, [], []
    excluded = path_listing(repo_root, NATIVE_GPU_BACKEND_LIST)
    for root in INTERFACE_ROOTS:
        for source in sorted((repo_root / root).rglob(SOURCE_SUFFIX)):
            rel = source.relative_to(repo_root).as_posix()
            # Before the node exists, which is the only place it can go. Dropping the
            # unit later would report it as `undeclared`, and dropping only its edges
            # would leave it in `path_of` for the staleness join to demand a scan entry
            # for a unit this configuration does not build.
            if rel in excluded:
                continue
            with open(source, "r", encoding="utf-8", errors="replace") as f:
                found = next(filter(None, map(MODULE_DECL.match, f)), None)
            if found is None:
                undeclared.append(rel)
                continue
            exported, name, partition = found.groups()
            logical = f"{name}:{partition}" if partition else name
            if logical in path_of:
                raise SystemExit(f"{logical} declared twice: "
                                 f"{path_of[logical]} and {rel}")
            path_of[logical] = rel
            if not exported:
                unexported.append(logical)
    return path_of, tuple(unexported), tuple(undeclared)


def scan_impls(repo_root: Path) -> tuple:
    """Module -> its implementation units for a wasm configuration; the undeclared ones.

    Keyed by MODULE and not by logical name, which is the one structural difference from
    `scan_interfaces`. An interface unit's logical name identifies it -- declaring one
    twice is the error that function raises on -- while a plain implementation unit says
    only `module gse.graphics;`, and there are a dozen of those per large module. So
    `gse.x:y_impl` and a nameless body of `gse.x` are both simply gse.x's, and the
    duplicate check cannot be made here.

    The selection is CMake's, through the three data files rather than through a second
    copy of its tests: the exclusions first, because a unit no configuration compiles is
    not an arm of anything, then one `kept_arm` per axis. Nothing is ordered here; see
    `impls_of`.

    A `.cpp` declaring no module is returned rather than dropped, for `scan_interfaces`'
    reason pointed the other way: it is a real translation unit of this engine that this
    walk cannot place, and a link built from a silently shortened list fails as a missing
    symbol -- the loudest possible symptom with its cause erased, in the one arm whose
    entire output is a list of missing symbols.
    """
    excluded = (path_listing(repo_root, EXCLUDED_IMPLS_LIST)
                | path_listing(repo_root, NATIVE_GPU_BACKEND_LIST))
    platform_arms = listing(repo_root, PLATFORM_ARMS_LIST)
    backend_arms = listing(repo_root, BACKEND_ARMS_LIST)
    impls, undeclared = collections.defaultdict(list), []
    for root in INTERFACE_ROOTS:
        for source in sorted((repo_root / root).rglob(IMPL_SUFFIX)):
            rel = source.relative_to(repo_root).as_posix()
            if rel in excluded:
                continue
            if not (kept_arm(rel, platform_arms, WASM_PLATFORM_ARM)
                    and kept_arm(rel, backend_arms, WASM_BACKEND_ARM)):
                continue
            with open(source, "r", encoding="utf-8", errors="replace") as f:
                found = next(filter(None, map(MODULE_DECL.match, f)), None)
            if found is None:
                undeclared.append(rel)
                continue
            impls[found.group(2)].append(rel)
    return {k: tuple(v) for k, v in impls.items()}, tuple(undeclared)


def scan_requires(build: Path) -> dict:
    """Logical name -> the engine names it requires, from CMake's dependency scan."""
    requires = {}
    # `*.cppm.ddi` and not `*.ddi`: there are two files per source in this tree,
    # `X.cppm.ddi` and `X.cppm.obj.ddi`, written by the scan and by the compile. This
    # glob takes the first spelling only, which is what keeps one source from being
    # read as two units.
    for ddi in sorted(build.rglob("*.cppm.ddi")):
        try:
            with open(ddi, "r", encoding="utf-8") as f:
                scan = json.load(f)
        except (OSError, json.JSONDecodeError):
            continue
        for rule in scan.get("rules", ()):
            needs = [req["logical-name"] for req in rule.get("requires", ())
                     if req.get("logical-name")]
            for provided in rule.get("provides", ()):
                logical = provided.get("logical-name")
                if logical:
                    requires[logical] = tuple(n for n in needs
                                              if n not in STANDARD_MODULES)
    return requires


def config_dir(repo_root: Path) -> Path:
    """The one configuration directory the scan is read from.

    Named rather than merged, and reported by every caller, because the whole reason to
    read `.ddi` instead of the source is that it has already resolved the preprocessor
    -- and merging two configurations' scans would union the arms back together.

    The newest of them when there is more than one, so that a tree carrying a stale
    configuration alongside a current one is read through the current one; which was
    read is printed, because no caller can tell from the table.
    """
    root = repo_root / DEFAULT_BUILD
    configs = [d for d in sorted(root.glob("*")) if (d / "CMakeCache.txt").exists()]
    if not configs:
        raise SystemExit(f"no configured build directory under {root}; "
                         f"the module graph is derived from its dependency scan")
    return max(configs, key=lambda d: d.stat().st_mtime)


def scan() -> Graph:
    """The graph, joined on the logical name, with both halves of the join reported."""
    chosen = config_dir(REPO_ROOT)
    path_of, unexported, undeclared = scan_interfaces(REPO_ROOT)
    impls, undeclared_impls = scan_impls(REPO_ROOT)
    requires = scan_requires(chosen)
    stale = tuple(sorted(logical for logical in path_of if logical not in requires))
    unresolved = {logical: tuple(sorted(req for req in needs if req not in path_of))
                  for logical, needs in requires.items() if logical in path_of}
    grouped = collections.defaultdict(list)
    for logical in sorted(path_of):
        grouped[module_of(logical)].append(logical)
    units_by_module = {k: tuple(v) for k, v in grouped.items()}
    # Module-level edges, for the frontier only. Undeclared imports -- the Vulkan SDK's
    # own `vulkan` -- are not edges to anywhere, so a module held up only by one of those
    # reaches no root and has to be reported from `external_requires` instead.
    imports = {module: {module_of(req)
                        for logical in units
                        for req in requires.get(logical, ())
                        if module_of(req) != module and module_of(req) in units_by_module}
               for module, units in units_by_module.items()}
    is_root = {module for module, units in units_by_module.items()
               if any(wraps_host(logical, path_of[logical]) for logical in units)}
    return Graph(build=chosen, path_of=path_of, units_by_module=units_by_module,
                 requires={k: v for k, v in requires.items() if k in path_of},
                 roots_of=host_roots(units_by_module, imports, is_root),
                 unexported=unexported, impls=impls,
                 undeclared_impls=undeclared_impls, undeclared=undeclared,
                 stale_interfaces=stale,
                 unresolved={k: v for k, v in unresolved.items() if v})


def note_for(graph: Graph, module: str) -> str:
    """Why MODULE is host-bound, or "" when it is not.

    The chain names a concrete route and the root set names the whole truth, so both are
    printed when they differ. The `external_requires` arm is the one this table was
    missing: a module held up only by an import the engine does not declare -- the
    Vulkan SDK's own `vulkan` -- reaches no root under Source/External and so has no
    chain, and rendering that as the empty note made it read exactly like a portable
    module. The sweep already said `via <name>` for this case; a derivation checked in
    two places has to be complete in both.
    """
    roots = graph.host_roots_of(module)
    if roots:
        # A root is in its own set, and a root that imports another -- gse.win32
        # .environment imports gse.win32 -- is in two. Saying so as "(+itself)" is how
        # that read before, so the module is taken out of its own answer first.
        chain = graph.host_chain(module)
        route = " -> ".join(chain[1:]) if len(chain) > 1 else "host root"
        others = sorted(roots - {module} - ({chain[-1]} if len(chain) > 1 else set()))
        return f"{route} (+{', '.join(others)})" if others else route
    external = sorted({name for logical in graph.units_of(module)
                       for name in graph.external_requires(logical)})
    return f"via {', '.join(external)}" if external else ""


def main() -> None:
    graph = scan()
    print(f"dependency scan: {graph.build}")
    print(f"module units in .cppm files: {len(graph.path_of)}, "
          f"{len(graph.unexported)} of them unexported partitions")
    if graph.undeclared:
        print(f"\n{len(graph.undeclared)} .cppm files declare no module and are not "
              f"in the graph:")
        for path in graph.undeclared:
            print(f"  {path}")
    if graph.stale_interfaces:
        print(f"\nnot in the dependency scan -- the build directory predates these "
              f"{len(graph.stale_interfaces)} files:")
        for logical in graph.stale_interfaces:
            print(f"  {logical:44} {graph.path_of[logical]}")
    if graph.unresolved:
        print("\nedges naming something no interface on disk provides:")
        for logical, names in sorted(graph.unresolved.items()):
            print(f"  {logical:44} {', '.join(names)}")
    if graph.undeclared_impls:
        print(f"\n{len(graph.undeclared_impls)} .cpp files declare no module, so no "
              f"module's link enumerates them:")
        for path in graph.undeclared_impls:
            print(f"  {path}")

    args = [arg for arg in sys.argv[1:] if arg != "--impls"]
    if args:
        impls = "--impls" in sys.argv[1:]
        for module in args:
            print(f"\n== {module}: {len(graph.closure(module))} interface units, "
                  f"in compile order ==")
            for path in graph.closure(module):
                print(f"  {path}")
            if impls:
                print(f"\n== {module}: {len(graph.impls_of(module))} implementation "
                      f"units for a wasm link, after all of the above ==")
                for path in graph.impls_of(module):
                    print(f"  {path}")
        return

    print("\n== host frontier ==")
    print(f"  {'root':28} {'sole':>5}  {'reached':>7}")
    for root, reached, sole in graph.frontier():
        print(f"  {root:28} {len(sole):5}  {len(reached):7}")

    print("\n== what has to be ported together ==")
    for roots, modules in graph.gated_groups():
        print(f"  {len(modules):3} modules behind {', '.join(sorted(roots))}")

    print(f"\n== {len(graph.modules())} modules ==")
    for module in graph.modules():
        note = note_for(graph, module)
        print(f"  {module:28} {len(graph.units_of(module)):3} units  "
              f"{len(graph.closure(module)):3} in closure  {note}")


if __name__ == "__main__":
    main()
