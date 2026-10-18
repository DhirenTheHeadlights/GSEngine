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
"""
import collections
import json
import re
import sys
from pathlib import Path
from typing import NamedTuple

from msys2 import REPO_ROOT

# Where interface units live. Import/ carries the primary interface of almost every
# module and Source/ carries the partitions, so neither alone is the engine.
INTERFACE_ROOTS = ("Engine/Engine/Source", "Engine/Engine/Import")

# The host frontier's roots, and they are a structural fact about the tree rather than a
# judgment about portability: every module under Source/External is a wrapper over a
# platform or vendored C library -- gse.win32, gse.directx, gse.freetype, gse.moodycamel
# and eleven more -- and there is no such thing as porting one. Everything else that is
# host-bound is host-bound *because its closure reaches one of these*, which is derived
# below and printed as a chain. Nothing is excused by being named in a list here.
HOST_DIR = "Engine/Engine/Source/External"

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


def module_of(logical: str) -> str:
    """The module a logical name belongs to: `gse.math:units` is gse.math's."""
    return logical.split(":", 1)[0]


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

    def sweep_order(self) -> tuple:
        """Every interface unit in the engine, dependencies first."""
        return self.order(sorted(self.path_of))

    def is_host_root(self, logical: str) -> bool:
        return self.path_of[logical].startswith(HOST_DIR)

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
    for root in INTERFACE_ROOTS:
        for source in sorted((repo_root / root).rglob(SOURCE_SUFFIX)):
            rel = source.relative_to(repo_root).as_posix()
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
               if any(path_of[logical].startswith(HOST_DIR) for logical in units)}
    return Graph(build=chosen, path_of=path_of, units_by_module=units_by_module,
                 requires={k: v for k, v in requires.items() if k in path_of},
                 roots_of=host_roots(units_by_module, imports, is_root),
                 unexported=unexported, undeclared=undeclared,
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

    if len(sys.argv) > 1:
        for module in sys.argv[1:]:
            print(f"\n== {module}: {len(graph.closure(module))} interface units, "
                  f"in compile order ==")
            for path in graph.closure(module):
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
