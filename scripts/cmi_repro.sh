#!/bin/bash
# Standalone reproducer for the GCC trunk CMI cluster corruption bug
set -e
cd /tmp/cmirepro
rm -f *.o *.gcm

GCC=/home/dhiren/gcc-trunk/bin/g++
FLAGS="-std=c++26 -fmodules-ts"
MODMAP_DIR=$(pwd)/.modules

mkdir -p "$MODMAP_DIR"

cat > "$MODMAP_DIR/mapper" << 'EOF'
$root .
m:base base.gcm
m:m_base m_base.gcm
m m.gcm
m_consumer m_consumer.gcm
EOF

build() {
    local src="$1"
    local out="${src%.cppm}.o"
    echo "[build] $src"
    $GCC $FLAGS \
        -fmodule-mapper="$MODMAP_DIR/mapper" \
        -c "$src" -o "$out"
}

cat > base.cppm << 'EOF'
module;
#include <memory>
export module m:base;

export namespace test {
    class node_base {
    public:
        virtual ~node_base() = default;
        virtual auto state_ptr() -> void* = 0;
        virtual auto state_ptr() const -> const void* = 0;
        virtual auto state_snapshot_ptr() const -> const void* = 0;
    };
}
EOF

cat > m_base.cppm << 'EOF'
module;
#include <memory>
#include <unordered_map>
#include <vector>
export module m:m_base;
import :base;

export namespace test {
    class scheduler {
    public:
        scheduler() = default;

        auto snapshot_ptr(int type) const -> const void*;
        auto frame_snapshot_ptr(int type) const -> const void*;

    private:
        std::vector<std::unique_ptr<node_base>> m_nodes;
        std::unordered_map<int, node_base*> m_state_index;
    };
}
EOF

cat > m.cppm << 'EOF'
export module m;
export import :base;
export import :m_base;
EOF

cat > m_consumer.cppm << 'EOF'
export module m_consumer;
import m;

export auto use_scheduler() -> void {
    test::scheduler s;
    (void)s.snapshot_ptr(0);
}
EOF

build base.cppm
build m_base.cppm
build m.cppm
echo "---"
echo "Now try to consume the umbrella:"
build m_consumer.cppm

echo "==> BASELINE PASSED (no corruption)"
