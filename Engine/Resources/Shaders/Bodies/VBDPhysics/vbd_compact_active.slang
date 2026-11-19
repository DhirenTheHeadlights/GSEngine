void vbd_compact_active(uint3 dispatch_id) {
    uint word = dispatch_id.x;
    uint word_count = (pc.body_count + 31u) / 32u;
    if (word >= word_count) return;

    uint bits = active_data[active_state_words + word];
    active_data[active_state_words + word] = 0;
    uint tail = pc.body_count - word * 32u;
    if (tail < 32u) {
        bits &= (1u << tail) - 1u;
    }
    uint count = countbits(bits);
    if (count == 0) return;

    uint base;
    InterlockedAdd(active_data[active_pending_word], count, base);
    uint half_base = (1u - active_data[active_half_word]) * max_bodies();
    while (bits != 0) {
        uint bit = firstbitlow(bits);
        bits &= bits - 1u;
        active_list[half_base + base] = word * 32u + bit;
        base++;
    }
}
