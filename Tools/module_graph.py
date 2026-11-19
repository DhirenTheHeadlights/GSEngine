"""The engine's module graph, read from a configured build directory.

Membership is CMake's: a unit is in the graph when the configuration's
`compile_commands.json` compiles it. Edges are the compiler's: each interface
unit's `.ddi` dependency scan, which has already run the preprocessor.

    python Tools/module_graph.py                 # the module table
    python Tools/module_graph.py gse.math        # one closure, in compile order
    python Tools/module_graph.py --impls gse     # and the impl units a link of it needs
"""
import collections
import json
import re
import sys
from pathlib import Path
from typing import NamedTuple

from repo_paths import BUILD_ROOT, REPO_ROOT

DEFAULT_CONFIG = BUILD_ROOT / "wasm32-wasi-gcc-Release"

STANDARD_MODULES = ("std", "std.compat")

MODULE_DECL = re.compile(
    r"^\s*(export\s+)?module\s+([A-Za-z_][\w.]*)\s*(?::\s*(\w+))?\s*;")


def module_of(logical: str) -> str:
    return logical.split(":", 1)[0]


class Graph(NamedTuple):
    build: Path
    path_of: dict
    requires: dict
    units_by_module: dict
    impls: dict
    undeclared: tuple
    unresolved: dict

    def modules(self) -> list:
        return sorted(self.units_by_module)

    def units_of(self, module: str) -> tuple:
        return self.units_by_module.get(module, ())

    def order(self, roots) -> tuple:
        """`roots` and everything they reach, dependencies first."""
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

    def paths(self, roots) -> tuple:
        return tuple(self.path_of[logical] for logical in self.order(roots))

    def partitions_of(self, module: str) -> list:
        return [logical for logical in self.units_of(module) if ":" in logical]

    def require_module(self, module: str) -> None:
        if module not in self.units_by_module:
            raise SystemExit(f"no module named {module} in {self.build}; "
                             f"{len(self.units_by_module)} modules were scanned")

    def closure(self, module: str) -> tuple:
        """Every interface unit needed to compile MODULE, in compile order."""
        self.require_module(module)
        return self.paths(self.units_of(module))

    def link_roots(self, module: str) -> tuple:
        """Every unit of every module MODULE's closure reaches, to a fixpoint."""
        reached = {module_of(logical) for logical in self.order(self.units_of(module))}
        while True:
            roots = tuple(logical for m in sorted(reached) for logical in self.units_of(m))
            grown = {module_of(logical) for logical in self.order(roots)}
            if grown == reached:
                return roots
            reached = grown

    def link_closure(self, module: str) -> tuple:
        return self.paths(self.link_roots(module))

    def impls_of(self, module: str) -> tuple:
        """Every implementation unit needed to link MODULE, after `link_closure`."""
        self.require_module(module)
        ordered, seen = [], set()
        for logical in self.order(self.link_roots(module)):
            owner = module_of(logical)
            if owner not in seen:
                seen.add(owner)
                ordered.extend(sorted(self.impls.get(owner, ())))
        return tuple(ordered)


def declaration(source: Path):
    with open(source, "r", encoding="utf-8", errors="replace") as f:
        return next(filter(None, map(MODULE_DECL.match, f)), None)


def compiled_units(build: Path) -> list:
    """(repo-relative source, dependency scan) for every unit the configuration compiles."""
    commands = build / "compile_commands.json"
    if not commands.exists():
        raise SystemExit(f"{commands} does not exist; configure that preset first")
    units = []
    for entry in json.loads(commands.read_text(encoding="utf-8")):
        source = Path(entry["file"]).resolve()
        if source.is_relative_to(REPO_ROOT):
            units.append((source.relative_to(REPO_ROOT).as_posix(),
                          Path(entry["output"] + ".ddi")))
    return sorted(units)


def scan_requires(ddi: Path) -> tuple:
    try:
        with open(ddi, "r", encoding="utf-8") as f:
            rules = json.load(f).get("rules", ())
    except (OSError, json.JSONDecodeError):
        return ()
    return tuple(req["logical-name"] for rule in rules
                 for req in rule.get("requires", ())
                 if req.get("logical-name") and req["logical-name"] not in STANDARD_MODULES)


def scan(build: Path = DEFAULT_CONFIG) -> Graph:
    path_of, requires, undeclared = {}, {}, []
    impls = collections.defaultdict(list)
    for rel, ddi in compiled_units(build):
        found = declaration(REPO_ROOT / rel)
        if found is None:
            undeclared.append(rel)
            continue
        _, name, partition = found.groups()
        if rel.endswith(".cppm"):
            logical = f"{name}:{partition}" if partition else name
            if logical in path_of:
                raise SystemExit(f"{logical} declared twice: {path_of[logical]} and {rel}")
            path_of[logical] = rel
            requires[logical] = scan_requires(ddi)
        else:
            impls[name].append(rel)
    grouped = collections.defaultdict(list)
    for logical in sorted(path_of):
        grouped[module_of(logical)].append(logical)
    unresolved = {logical: tuple(sorted(req for req in needs if req not in path_of))
                  for logical, needs in requires.items()}
    return Graph(build=build, path_of=path_of, requires=requires,
                 units_by_module={k: tuple(v) for k, v in grouped.items()},
                 impls={k: tuple(v) for k, v in impls.items()},
                 undeclared=tuple(undeclared),
                 unresolved={k: v for k, v in unresolved.items() if v})


def main() -> None:
    graph = scan()
    print(f"configuration: {graph.build}")
    print(f"interface units: {len(graph.path_of)}")
    if graph.undeclared:
        print(f"\n{len(graph.undeclared)} compiled files declare no module:")
        for path in graph.undeclared:
            print(f"  {path}")
    if graph.unresolved:
        print("\nedges naming something no compiled interface provides:")
        for logical, names in sorted(graph.unresolved.items()):
            print(f"  {logical:44} {', '.join(names)}")

    args = [arg for arg in sys.argv[1:] if arg != "--impls"]
    if args:
        impls = "--impls" in sys.argv[1:]
        for module in args:
            closure = graph.closure(module)
            print(f"\n== {module}: {len(closure)} interface units, in compile order ==")
            for path in closure:
                print(f"  {path}")
            if impls:
                bodies = graph.impls_of(module)
                print(f"\n== {module}: {len(bodies)} implementation units ==")
                for path in bodies:
                    print(f"  {path}")
        return

    print(f"\n== {len(graph.modules())} modules ==")
    for module in graph.modules():
        print(f"  {module:28} {len(graph.units_of(module)):3} units  "
              f"{len(graph.closure(module)):3} in closure")


if __name__ == "__main__":
    main()
