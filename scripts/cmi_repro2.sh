#!/bin/bash
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
    $GCC $FLAGS -fmodule-mapper="$MODMAP_DIR/mapper" -c "$src" -o "$out"
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

    template <typename S, typename State>
    class node_t final : public node_base {
    public:
        template <typename... Args>
        explicit node_t(Args&&... args) : m_state(std::forward<Args>(args)...) {}
        auto state_ptr() -> void* override { return &m_state; }
        auto state_ptr() const -> const void* override { return &m_state; }
        auto state_snapshot_ptr() const -> const void* override { return &m_state; }
    private:
        State m_state;
    };
}
EOF

cat > m_base.cppm << 'EOF'
module;
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
export module m:m_base;
import :base;

export namespace test {
    class scheduler {
    public:
        scheduler() = default;

        auto snapshot_ptr(int type) const -> const void*;
        auto frame_snapshot_ptr(int type) const -> const void*;

        template <typename S, typename State, typename... Args>
        auto add(Args&&... args) -> State&;

        auto push_deferred(std::function<void()> fn) -> void;

    private:
        std::vector<std::unique_ptr<node_base>> m_nodes;
        std::unordered_map<int, node_base*> m_state_index;
        std::vector<std::function<void()>> m_deferred;
        std::mutex m_deferred_mutex;
    };
}

template <typename S, typename State, typename... Args>
auto test::scheduler::add(Args&&... args) -> State& {
    auto ptr = std::make_unique<node_t<S, State>>(std::forward<Args>(args)...);
    auto* raw = ptr.get();
    m_state_index.emplace(0, raw);
    m_nodes.push_back(std::move(ptr));
    return *static_cast<State*>(raw->state_ptr());
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

struct my_system {};
struct my_state {};

export auto use_scheduler() -> void {
    test::scheduler s;
    s.add<my_system, my_state>();
    (void)s.snapshot_ptr(0);
}
EOF

build base.cppm
build m_base.cppm
build m.cppm
echo "---"
echo "Consume:"
build m_consumer.cppm

echo "==> SCENARIO 2 PASSED"
