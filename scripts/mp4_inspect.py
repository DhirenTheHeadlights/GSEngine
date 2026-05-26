import struct
import sys
from pathlib import Path


def read_u32(buf, off):
    return struct.unpack_from(">I", buf, off)[0]


def read_u64(buf, off):
    return struct.unpack_from(">Q", buf, off)[0]


def read_leb128(buf, off):
    value = 0
    consumed = 0
    for i in range(8):
        if off + i >= len(buf):
            return None
        b = buf[off + i]
        value |= (b & 0x7F) << (i * 7)
        consumed += 1
        if (b & 0x80) == 0:
            return value, consumed
    return None


def walk_boxes(buf, start, end, depth=0):
    off = start
    while off + 8 <= end:
        size = read_u32(buf, off)
        fourcc = buf[off + 4:off + 8].decode("ascii", errors="replace")
        header_size = 8
        if size == 1:
            size = read_u64(buf, off + 8)
            header_size = 16
        elif size == 0:
            size = end - off
        print(f"{'  ' * depth}{fourcc} @0x{off:x} size={size}")
        body_start = off + header_size
        body_end = off + size
        if fourcc in ("moov", "trak", "mdia", "minf", "stbl", "dinf", "edts",
                      "mvex", "moof", "traf"):
            walk_boxes(buf, body_start, body_end, depth + 1)
        elif fourcc == "stsd":
            inner = body_start + 8
            walk_boxes(buf, inner, body_end, depth + 1)
        elif fourcc == "av01":
            inner = body_start + 78
            walk_boxes(buf, inner, body_end, depth + 1)
        elif fourcc == "av1C":
            cfg = buf[body_start:body_end]
            print(f"{'  ' * (depth + 1)}av1C bytes: {cfg.hex(' ')}")
            if len(cfg) >= 4:
                b0, b1, b2, b3 = cfg[0], cfg[1], cfg[2], cfg[3]
                print(f"{'  ' * (depth + 1)}marker={b0 >> 7} version={b0 & 0x7F}")
                print(f"{'  ' * (depth + 1)}seq_profile={b1 >> 5} seq_level_idx_0={b1 & 0x1F}")
                print(f"{'  ' * (depth + 1)}tier={b2 >> 7} high_bitdepth={(b2 >> 6) & 1} "
                      f"twelve_bit={(b2 >> 5) & 1} monochrome={(b2 >> 4) & 1}")
                print(f"{'  ' * (depth + 1)}subsampling=({(b2 >> 3) & 1},{(b2 >> 2) & 1}) "
                      f"chroma_sample_position={b2 & 0x3}")
                obus = cfg[4:]
                print(f"{'  ' * (depth + 1)}configOBUs: {obus.hex(' ')}")
                parse_obus(obus, depth + 1)
        elif fourcc in ("mvhd", "tkhd", "mdhd"):
            ver = buf[body_start]
            if ver == 1:
                if fourcc == "mvhd":
                    dur = read_u64(buf, body_start + 4 + 8 + 8 + 4)
                    ts = read_u32(buf, body_start + 4 + 8 + 8)
                elif fourcc == "tkhd":
                    dur = read_u64(buf, body_start + 4 + 8 + 8 + 4 + 4)
                    ts = None
                else:
                    dur = read_u64(buf, body_start + 4 + 8 + 8 + 4)
                    ts = read_u32(buf, body_start + 4 + 8 + 8)
                print(f"{'  ' * (depth + 1)}version=1 duration={dur} timescale={ts}")
        elif fourcc == "tfhd":
            flags = read_u32(buf, body_start) & 0xFFFFFF
            track_id = read_u32(buf, body_start + 4)
            print(f"{'  ' * (depth + 1)}track_ID={track_id} flags=0x{flags:06x}")
        elif fourcc == "tfdt":
            ver = buf[body_start]
            if ver == 1:
                base = read_u64(buf, body_start + 4)
            else:
                base = read_u32(buf, body_start + 4)
            print(f"{'  ' * (depth + 1)}baseMediaDecodeTime={base}")
        elif fourcc == "trun":
            flags = read_u32(buf, body_start) & 0xFFFFFF
            count = read_u32(buf, body_start + 4)
            cursor = body_start + 8
            data_offset = None
            if flags & 0x000001:
                data_offset = struct.unpack_from(">i", buf, cursor)[0]
                cursor += 4
            if flags & 0x000004:
                cursor += 4
            print(f"{'  ' * (depth + 1)}flags=0x{flags:06x} count={count} data_offset={data_offset}")
            print(f"{'  ' * (depth + 1)}first 3 samples:")
            for i in range(min(3, count)):
                dur_field = struct.unpack_from(">I", buf, cursor)[0] if flags & 0x000100 else None
                if flags & 0x000100:
                    cursor += 4
                size_field = struct.unpack_from(">I", buf, cursor)[0] if flags & 0x000200 else None
                if flags & 0x000200:
                    cursor += 4
                flags_field = struct.unpack_from(">I", buf, cursor)[0] if flags & 0x000400 else None
                if flags & 0x000400:
                    cursor += 4
                print(f"{'  ' * (depth + 2)}sample[{i}] dur={dur_field} size={size_field} "
                      f"flags=0x{flags_field:08x}" if flags_field is not None else f"sample[{i}]")
        off += size


def parse_obus(buf, depth):
    obu_names = {
        0: "reserved", 1: "OBU_SEQUENCE_HEADER", 2: "OBU_TEMPORAL_DELIMITER",
        3: "OBU_FRAME_HEADER", 4: "OBU_TILE_GROUP", 5: "OBU_METADATA",
        6: "OBU_FRAME", 7: "OBU_REDUNDANT_FRAME_HEADER", 8: "OBU_TILE_LIST",
        15: "OBU_PADDING",
    }
    i = 0
    while i < len(buf):
        header = buf[i]
        obu_type = (header >> 3) & 0x0F
        ext = (header >> 2) & 1
        has_size = (header >> 1) & 1
        pos = i + 1
        if ext:
            pos += 1
        payload_size = 0
        if has_size:
            r = read_leb128(buf, pos)
            if not r:
                print(f"{'  ' * depth}<bad leb128>")
                break
            payload_size, consumed = r
            pos += consumed
        else:
            payload_size = len(buf) - pos
        end = pos + payload_size
        if end > len(buf):
            print(f"{'  ' * depth}<obu type={obu_type} truncated end={end} len={len(buf)}>")
            break
        name = obu_names.get(obu_type, f"type{obu_type}")
        print(f"{'  ' * depth}{name} payload={payload_size}B raw_payload={buf[pos:end].hex(' ')}")
        if obu_type == 1:
            decode_seq_header(buf[pos:end], depth + 1)
        i = end


def decode_seq_header(payload, depth):
    bit_pos = 0

    def read_bits(n):
        nonlocal bit_pos
        v = 0
        for _ in range(n):
            byte_idx = bit_pos // 8
            bit_idx = 7 - (bit_pos % 8)
            if byte_idx >= len(payload):
                return v
            v = (v << 1) | ((payload[byte_idx] >> bit_idx) & 1)
            bit_pos += 1
        return v

    seq_profile = read_bits(3)
    still = read_bits(1)
    reduced = read_bits(1)
    print(f"{'  ' * depth}seq_profile={seq_profile} still={still} reduced={reduced}")
    if reduced:
        return
    timing = read_bits(1)
    if timing:
        print(f"{'  ' * depth}timing_info_present, skipping")
        return
    init_delay = read_bits(1)
    op_cnt = read_bits(5)
    print(f"{'  ' * depth}init_delay_present={init_delay} op_cnt_minus_1={op_cnt}")
    op_idc = read_bits(12)
    level_idx = read_bits(5)
    tier = read_bits(1) if level_idx > 7 else 0
    print(f"{'  ' * depth}op_idc[0]={op_idc} level_idx_0={level_idx} tier_0={tier}")
    fwb = read_bits(4)
    fhb = read_bits(4)
    print(f"{'  ' * depth}frame_width_bits_minus_1={fwb} frame_height_bits_minus_1={fhb}")
    max_w = read_bits(fwb + 1)
    max_h = read_bits(fhb + 1)
    print(f"{'  ' * depth}max_frame_width_minus_1={max_w} max_frame_height_minus_1={max_h}")


def main():
    path = Path(sys.argv[1])
    buf = path.read_bytes()
    print(f"file size: {len(buf)} bytes")
    walk_boxes(buf, 0, len(buf))


if __name__ == "__main__":
    main()
