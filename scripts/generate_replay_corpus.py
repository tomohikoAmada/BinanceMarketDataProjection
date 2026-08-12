#!/usr/bin/env python3
"""Generate M5 Phase-5 replay fuzz corpus seeds.

Each seed encodes a structured replay test case for a mandatory fuzz category.
The encoding matches the ByteCursor/read_var_u64/read_string/read_bounded format
in fuzz/m5/replay_fuzz_decoder.cpp.
"""
import os
import struct

REPLAY_CORPUS = os.path.join(os.path.dirname(__file__), "..", "fuzz", "corpus", "replay")


def var_u64(value):
    """Encode a uint64 for ByteCursor::read_var_u64.

    Special values use the special flag:
      special=0: 0, special=1: 1, special=2: 2,
      special=3: UINT64_MAX, special=4: UINT64_MAX-1, special=5: UINT64_MAX-2

    Normal values are encoded as big-endian with minimal width byte count.
    """
    if value == 0:
        return bytes([0x08])  # special flag | special_idx=0
    if value == 1:
        return bytes([0x18])  # special flag | special_idx=1
    if value == 2:
        return bytes([0x28])  # special flag | special_idx=2
    if value == 0xFFFFFFFFFFFFFFFF:
        return bytes([0x38])
    if value == 0xFFFFFFFFFFFFFFFE:
        return bytes([0x48])
    if value == 0xFFFFFFFFFFFFFFFD:
        return bytes([0x58])

    # Determine minimal width
    w = 0
    while (value >> (8 * (w + 1))) > 0:
        w += 1
    # byte: [0:2] = width, [3] = 0 (not special)
    header = w  # width-1 is already in bits 0:2
    result = bytes([header])
    for i in range(w, -1, -1):
        result += bytes([(value >> (8 * i)) & 0xFF])
    return result


def level_token(value_str):
    """Encode a level token for decode_level_token."""
    if not value_str:
        return bytes([0x12])  # mode 2: empty
    if value_str == "0":
        return bytes([0x1A])  # mode 2 but 0...
        # Actually let me use mode 1 (pure integer) for simplicity
    # mode 1: pure integer
    val = int(value_str)
    return bytes([0x01, val & 0xFF])


def decimal_token(num_str):
    """Encode a decimal-form level token (mode 0)."""
    s = str(num_str)
    if "." in s:
        int_part, frac_part = s.split(".", 1)
    else:
        int_part, frac_part = s, ""
    int_len = min(len(int_part), 4)
    has_frac = 1 if frac_part else 0
    frac_len = min(len(frac_part), 3)
    hdr = (int_len & 0x0F) | ((has_frac & 1) << 4) | ((frac_len & 3) << 5)
    result = bytes([0x00, hdr])
    for d in int_part[:int_len]:
        val = (ord(d) - ord('0')) % 10
        result += bytes([val])
    if has_frac:
        for d in frac_part[:frac_len]:
            val = (ord(d) - ord('0')) % 10
            result += bytes([val])
    return result


def string_with_len(s, pad=0):
    """Encode a string: 1-byte length, then bytes."""
    data = s.encode("ascii", errors="replace")
    if len(data) > 255:
        data = data[:255]
    return bytes([len(data)]) + data + bytes([pad] * (pad > 0))


def levels_data(levels):
    """Encode bids/asks levels for decode_levels."""
    count = min(len(levels), 8)
    data = bytes([count])
    for side, price, qty in levels:
        data += bytes([0 if side == "B" else 1])
        data += decimal_token(price)
        data += decimal_token(qty)
    return data


def quality_fact_byte(quality_idx):
    """Encode a quality fact value for decode_quality_fact."""
    # Maps to the 13-value map via read_bounded(12)
    return bytes([quality_idx % 13])


def quality_facts(facts):
    """Encode quality facts: count byte then each fact."""
    count = min(len(facts), 6)
    return bytes([count]) + b"".join(quality_fact_byte(f) for f in facts)


def make_seed(filename, data):
    path = os.path.join(REPLAY_CORPUS, filename)
    os.makedirs(REPLAY_CORPUS, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print(f"  {filename}: {len(data)} bytes")


def header(market="spot", adapter=False, pscale=8, qscale=8, symbol="BTCUSDT"):
    """Create the header bytes."""
    hdr = 0
    if adapter:
        hdr |= 0x01  # bit 0: adapter mode
    if market == "usdm":
        hdr |= 0x02  # bit 1: usdm market
    hdr |= ((pscale & 0x3F) << 2)  # bits 2-7: price_scale
    result = bytes([hdr, qscale & 0xFF])
    result += string_with_len(symbol)
    return result


def install_baseline_op(last_update_id, bids, asks):
    """Opcode 0: InstallBaseline."""
    data = bytes([0x00])            # opcode 0
    data += var_u64(last_update_id)
    data += levels_data(bids)
    data += levels_data(asks)
    return data


def depth_update_op(first_id, final_id, previous, levels):
    """Opcode 1: DepthUpdate."""
    data = bytes([0x01])            # opcode 1
    data += var_u64(first_id)
    data += var_u64(final_id)
    has_prev = 1 if previous is not None else 0
    data += bytes([has_prev & 1])
    if previous is not None:
        data += var_u64(previous)
    data += levels_data(levels)
    return data


def rebaseline_op(last_update_id, bids, asks):
    """Opcode 2: Rebaseline."""
    data = bytes([0x02])
    data += var_u64(last_update_id)
    data += levels_data(bids)
    data += levels_data(asks)
    return data


def reset_op():
    """Opcode 3: Reset."""
    return bytes([0x03])


def snapshot_request_op(depth_limit, host_quality, snapshot_id, producer,
                        producer_version, origin, generated_time,
                        has_monotonic=False, monotonic_ns=0,
                        has_gap=False, gap_seq=0, gap_state=0):
    """Opcode 4: SnapshotRequest."""
    data = bytes([0x04])
    if depth_limit is not None:
        data += bytes([0x01, depth_limit & 0xFF])
    else:
        data += bytes([0x00])
    data += quality_facts(host_quality)
    data += string_with_len(snapshot_id)
    data += string_with_len(producer)
    data += string_with_len(producer_version)
    origin_map = {"gateway": 0, "recorder": 1, "history": 2}
    data += bytes([origin_map.get(origin, 0)])
    data += var_u64(generated_time)
    data += bytes([0x01 if has_monotonic else 0x00])
    if has_monotonic:
        data += var_u64(monotonic_ns)
    data += bytes([0x01 if has_gap else 0x00])
    if has_gap:
        data += var_u64(gap_seq)
        data += bytes([gap_state % 5])
    return data


def adapter_metadata_op(quality):
    """Opcode 5: AdapterMetadata."""
    data = bytes([0x05])
    data += quality_facts(quality)
    return data


def malformed_range_op(first_id, final_id):
    """Opcode 6: MalformedRange with first > final."""
    data = bytes([0x06])
    data += var_u64(first_id)
    data += var_u64(final_id)
    return data


# ── Category 1: Spot synchronized stream ──
def gen_spot_synchronized():
    ops = b""
    ops += install_baseline_op(100, [("B", "50000", "1.5")], [("A", "50100", "2.0")])
    ops += depth_update_op(101, 101, None, [("B", "50010", "1.0")])
    ops += depth_update_op(102, 102, None, [("A", "50090", "3.0")])
    data = header("spot") + bytes([3]) + ops
    make_seed("spot_synchronized_stream.bin", data)


# ── Category 2: USD-M synchronized stream ──
def gen_usdm_synchronized():
    ops = b""
    ops += install_baseline_op(200, [("B", "50000", "1.5")], [("A", "50100", "2.0")])
    ops += depth_update_op(200, 200, 200, [("B", "50010", "1.0")])  # bridge
    ops += depth_update_op(205, 206, 200, [("A", "50090", "3.0")])
    data = header("usdm") + bytes([3]) + ops
    make_seed("usdm_synchronized_stream.bin", data)


# ── Category 3: bridge transition ──
def gen_bridge_transition():
    ops = b""
    ops += install_baseline_op(50, [("B", "60000", "1.0")], [("A", "60100", "1.5")])
    # Bridge: exact successor
    ops += depth_update_op(51, 51, None, [("B", "60000", "0")])
    data = header("spot") + bytes([2]) + ops
    make_seed("bridge_transition.bin", data)


# ── Category 4: gap ──
def gen_gap():
    ops = b""
    ops += install_baseline_op(50, [("B", "60000", "1.0")], [("A", "60100", "1.5")])
    # Forward gap: first >> current+1
    ops += depth_update_op(100, 101, None, [("B", "60100", "2.0")])
    data = header("spot") + bytes([2]) + ops
    make_seed("gap.bin", data)


# ── Category 5: recovery ──
def gen_recovery():
    ops = b""
    ops += install_baseline_op(50, [("B", "60000", "1.0")], [("A", "60100", "1.5")])
    ops += depth_update_op(100, 101, None, [("B", "60100", "2.0")])
    ops += reset_op()
    ops += rebaseline_op(200, [("B", "61000", "2.0")], [("A", "61100", "3.0")])
    data = header("spot") + bytes([4]) + ops
    make_seed("recovery.bin", data)


# ── Category 6: duplicate/stale ──
def gen_duplicate_stale():
    ops = b""
    ops += install_baseline_op(50, [("B", "60000", "1.0")], [("A", "60100", "1.5")])
    ops += depth_update_op(51, 51, None, [("B", "60001", "0.5")])  # Successor
    ops += depth_update_op(51, 51, None, [("B", "60002", "1.0")])  # Duplicate
    data = header("spot") + bytes([3]) + ops
    make_seed("duplicate_stale.bin", data)


# ── Category 7: locked/crossed ──
def gen_locked_crossed():
    ops = b""
    ops += install_baseline_op(50,
        [("B", "50000", "1.0"), ("B", "49900", "2.0")],
        [("A", "49900", "1.5"), ("A", "50000", "2.5")])
    data = header("spot") + bytes([1]) + ops
    make_seed("locked_crossed.bin", data)


# ── Category 8: decimal boundaries ──
def gen_decimal_boundaries():
    ops = b""
    # Levels with varied decimal forms: empty, 0, integer, decimal point, fractional,
    # leading zeros, large magnitude
    ops += install_baseline_op(1,
        [("B", "", "0"), ("B", "0", "1"), ("B", "1", "1.5"), ("B", "9", "00"),
         ("B", "9999999999", "9999999999")],
        [("A", "1", "1"), ("A", "1.23", "0.001"), ("A", "9999999999999999999", "1")])
    data = header("spot") + bytes([1]) + ops
    make_seed("decimal_boundaries.bin", data)


# ── Category 9: depth-limit snapshot ──
def gen_depth_limit_snapshot():
    ops = b""
    ops += install_baseline_op(50, [("B", "60000", "1.0")], [("A", "60100", "1.5")])
    ops += depth_update_op(51, 51, None, [("B", "60010", "2.0")])
    # Snapshot with depth limit
    ops += snapshot_request_op(
        depth_limit=5,
        host_quality=[0, 1],
        snapshot_id="abc123",
        producer="fuzz",
        producer_version="1",
        origin="gateway",
        generated_time=1000000,
        has_monotonic=True,
        monotonic_ns=500000,
        has_gap=False,
    )
    ops += snapshot_request_op(
        depth_limit=None,
        host_quality=[],
        snapshot_id="no-limit",
        producer="fuzz",
        producer_version="1",
        origin="history",
        generated_time=2000000,
        has_gap=True,
        gap_seq=50,
        gap_state=1,  # ResyncRequired
    )
    data = header("spot") + bytes([4]) + ops
    make_seed("depth_limit_snapshot.bin", data)


# ── Category 10: quality combinations ──
def gen_quality_combinations():
    ops = b""
    ops += adapter_metadata_op([0, 1, 2, 3, 4])
    ops += install_baseline_op(50, [("B", "60000", "1.0")], [("A", "60100", "1.5")])
    ops += snapshot_request_op(
        depth_limit=10,
        host_quality=[0, 3, 6, 9, 12],
        snapshot_id="qcomb1",
        producer="fuzz",
        producer_version="1",
        origin="recorder",
        generated_time=3000000,
        has_gap=True,
        gap_seq=50,
        gap_state=3,  # Recovered
    )
    data = header("spot") + bytes([3]) + ops
    make_seed("quality_combinations.bin", data)


def main():
    print("Generating M5 replay fuzz corpus seeds...")
    gen_spot_synchronized()
    gen_usdm_synchronized()
    gen_bridge_transition()
    gen_gap()
    gen_recovery()
    gen_duplicate_stale()
    gen_locked_crossed()
    gen_decimal_boundaries()
    gen_depth_limit_snapshot()
    gen_quality_combinations()
    print("Done.")


if __name__ == "__main__":
    main()
