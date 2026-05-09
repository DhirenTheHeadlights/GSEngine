# Network & Server Module Audit

Audit of `Engine/Engine/Source/Network/` and `Server/Server/Include/` on branch `forward-plus-renderer-refactor` (2026-05-09). All file:line references verified against the working tree at audit time — re-verify before acting.

Scope: anti-patterns, dead/legacy code, and boilerplate eliminable via p2996 reflection. Public API surface and wire format intentionally preserved unless noted.

---

## 1. Dead code (cut, zero functionality loss)

### 1.1 `server::process_header`
- Decl: `Server/Server/Include/Server.cppm:80`
- Def: `Server/Server/Include/Server.cppm:542`
- Never called. Identical logic inlined at `Server/Server/Include/Server.cppm:371` and `Engine/Engine/Source/Network/Client.cppm:208`.

### 1.2 `bitstream` per-message-id tracking
- `Engine/Engine/Source/Network/Bitstream.cppm:78` `set_current_message_id`
- `Engine/Engine/Source/Network/Bitstream.cppm:82` `current_message_id`
- `Engine/Engine/Source/Network/Bitstream.cppm:97` `m_cur_msg_id`
- `Engine/Engine/Source/Network/Bitstream.cppm:241`–`246` defs
- The public setter/getter have no callers. The field is only consumed by an internal `assert` format string at `Engine/Engine/Source/Network/Bitstream.cppm:118`. If the assert context is wanted, set the field internally on `write(id)` paths and drop the public API.

### 1.3 `bitstream::reader` static factory
- Decl: `Engine/Engine/Source/Network/Bitstream.cppm:21`
- Def: `Engine/Engine/Source/Network/Bitstream.cppm:103`
- No callers. Every site constructs `bitstream(data)` directly.

### 1.4 `remote_peer::pending_reliable_count`
- Decl: `Engine/Engine/Source/Network/RemotePeer.cppm:52`
- Def: `Engine/Engine/Source/Network/RemotePeer.cppm:128`
- No callers.

### 1.5 `remote_peer::m_last_processed_ack`
- `Engine/Engine/Source/Network/RemotePeer.cppm:63`
- Written at line 112, never read.

### 1.6 Periodic position log
- `Server/Server/Include/Server.cppm:488`–`506`
- `static std::uint32_t s_frame_counter` plus `std::println` every 120 frames. Smells like leftover debugging that escaped a commit.

---

## 2. Anti-patterns

### 2.1 Triplicated ack-bitfield update
Same logic in three places:
- `Engine/Engine/Source/Network/Client.cppm:208`–`222`
- `Server/Server/Include/Server.cppm:371`–`385`
- `Server/Server/Include/Server.cppm:545`–`558` (the dead `process_header`)

Move into a `remote_peer::ingest_packet_sequence(std::uint32_t)` member alongside `process_acks`.

### 2.2 `server::send` and `server::send_reliable` are ~95% duplicates
- `Server/Server/Include/Server.cppm:148`
- `Server/Server/Include/Server.cppm:177`

Reliable variant only adds `queue_reliable` and captures size. Unify with a `bool reliable` flag or have `send_reliable` wrap `send`.

### 2.3 `connection_request` accept and reconnect paths duplicated
- New peer: `Server/Server/Include/Server.cppm:285`–`322`
- Reconnect: `Server/Server/Include/Server.cppm:325`–`365`

Both: controller create + `connection_accepted` + scene-change + snapshot-pending. Extract `accept_connection(addr, reuse_existing)`.

### 2.4 Magic XOR for remove-message id
- `Engine/Engine/Source/Network/Message/RegistrySync.cppm:117`: `stable_code(name) ^ 0x5A5Au`

Brittle and undocumented. Hash a distinct string instead, e.g. `stable_code(std::format("{}:remove", name))`.

### 2.5 Resend queues encoded packet bytes, not the message
- `Server/Server/Include/Server.cppm:198` queues bytes
- `Server/Server/Include/Server.cppm:213`–`227` strips the header by hardcoding `sizeof(packet_header)` and re-encodes a new one in front

Cleaner: queue the message body (or the typed message) and re-encode end-to-end on resend.

### 2.6 `inbox_message` variant duplicates the dispatch chain
- Variant list: `Engine/Engine/Source/Network/Client.cppm:37`–`44`
- Dispatch chain: `Engine/Engine/Source/Network/Client.cppm:227`–`261`

Two parallel hand-maintained lists of the same message types. See §3.1 for the reflection fix.

### 2.7 Discovery uses string-keyed map
- `Engine/Engine/Source/Network/Discovery.cppm:124`, `134`, `149` build keys via `ip + ":" + port`
- `address` already has `operator<=>` (`Engine/Engine/Source/Network/Socket.cppm:20`)

Use `std::map<address, …>` directly.

### 2.8 `task::group` per replication call
- `Engine/Engine/Source/Network/Replication.cppm:158` (`replicate_deltas`)
- `Engine/Engine/Source/Network/Replication.cppm:175` (`replicate_snapshot_to`)

Spawning a task group per-component-type per-tick for typically tiny payloads is probably net negative. Worth profiling before keeping.

---

## 3. Reflection opportunities (p2996)

The reflective encode/decode pattern is already half-built in `Engine/Engine/Source/Network/Message/Message.cppm:175`–`189`. Push it the rest of the way.

### 3.1 Auto-generate exhaustive message dispatch
Replaces:
- `Engine/Engine/Source/Network/Client.cppm:227`–`261`
- `Server/Server/Include/Server.cppm:387`–`450`
- `Engine/Engine/Source/Network/Client.cppm:37`–`44` (`inbox_message` variant)

Walk every type in `gse::network` annotated `[[= network_message{}]]` and dispatch on `message_id`:

```cpp
template <typename Visitor>
auto dispatch_network_message(bitstream& s, std::uint16_t id, Visitor&& v) -> bool {
    bool handled = false;
    template for (constexpr auto r : std::define_static_array(
        std::meta::members_of(^^gse::network, std::meta::access_context::unchecked())))
    {
        if constexpr (std::meta::is_type(r)) {
            using T = typename [: r :];
            if constexpr (is_network_message<T>()) {
                if (!handled && id == message_id(std::type_identity<T>{})) {
                    v(decode(s, std::type_identity<T>{}));
                    handled = true;
                }
            }
        }
    }
    return handled;
}
```

`inbox_message` becomes a reflection-built variant of the same type set.

### 3.2 Auto-discover networked components
Replaces:
- `Engine/Engine/Source/Network/Message/RegistrySync.cppm:92`–`97` (`networked_types` tuple)

Components already carry `[[= networked]]` on their fields (`Engine/Engine/Source/Meta/Annotations.cppm:8`–`12`, predicate at `Engine/Engine/Source/Network/Replication.cppm:54`). Tag the *types* with `[[= networked_component]]` and reflect over `gse` / `gse::physics` to build the list. Adding a new networked component stops being a two-place edit.

### 3.3 Make `component_upsert<T>` / `component_remove<T>` real network messages
Deletes:
- `Engine/Engine/Source/Network/Message/RegistrySync.cppm:127`–`155` (four hand-written encode/decode bodies)

Annotate the templates `[[= network_message{}]]`. The generic reflective `encode`/`decode` at `Engine/Engine/Source/Network/Message/Message.cppm:175`–`189` then handles them.

### 3.4 Unify message-id derivation
- `Engine/Engine/Source/Network/Message/Message.cppm:169` hashes `type_tag<T>()`
- `Engine/Engine/Source/Network/Message/RegistrySync.cppm:111` hashes `meta::qualified_name<T>()`

Same intent, different inputs. Pick one and route everywhere through it via reflection — drop the parallel `component_name` / `component_code_*` layer.

### 3.5 Reflect the input-frame tail
Hand-rolled tail today:
- Write: `Engine/Engine/Source/Network/Actions.cppm:83`–`101` (3 `uint64_t` word loops + 2 pair-vector loops)
- Read: `Server/Server/Include/Server.cppm:399`–`450` (mirror)

Add `bitstream::write(std::span<const T>)` / `read_vector<T>(n)` and emit via `template for` over the message struct's fields. Then `input_frame` becomes a single `[[= network_message]]` struct with `std::vector<…>` members and uses the generic encode/decode.

---

## Suggested ordering

Low-risk to high-risk. Each step is independently committable.

1. **Delete dead code** (§1.1–§1.5). Pure deletion. Optionally drop §1.6 if confirmed not load-bearing.
2. **Move ack-bitfield update onto `remote_peer`** (§2.1). Call from client + server.
3. **Annotate `component_upsert` / `component_remove` as `network_message`** (§3.3) and drop bespoke encode/decode.
4. **Replace `match_message` chains with reflective dispatcher** (§3.1); collapse `inbox_message` accordingly.
5. **Auto-discover networked component types** (§3.2).
6. **Unify message-id derivation** (§3.4).
7. **Reflect input-frame tail** (§3.5) — touches wire format if encoding changes; do last and bump a protocol version if needed.
8. **Refactor send/send_reliable and connection accept/reconnect duplication** (§2.2, §2.3) — independent of the rest, can land any time.
9. Lower priority: §2.4 magic XOR, §2.5 resend-by-bytes, §2.7 discovery string keys, §2.8 task groups.
