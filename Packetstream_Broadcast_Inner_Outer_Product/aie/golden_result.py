import re
from pathlib import Path
from typing import List

# ---------- Dimensions: must match aie_core1.cpp F_* ----------
F_Ra = 16
F_Ca = 8
F_Rb = F_Ca
F_Cb = 8

ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"
OUT_DIR = ROOT / "aiesimulator_output"
OUT_DIR.mkdir(parents=True, exist_ok=True)

IN0 = DATA_DIR / "input0.seq"
IN1 = DATA_DIR / "input1.txt"
OUT = OUT_DIR / "output1golden.txt"

# Timestamp generation (ps)
DEFAULT_START_TS = 6553600     
DEFAULT_TS_STEP = 28800       
base_timestamps: List[int] = []


# ----------------- helper parsing -----------------
def tokenize_keep_markers(txt: str):
    return [t for t in re.split(r"[\s,]+", txt.strip()) if t != ""]


def try_parse_int(tok: str):
    if tok.startswith(("0x", "-0x")):
        return int(tok, 16)
    clean = re.sub(r"[^0-9\-]+$", "", tok)
    return int(clean)


def read_A_packets_ignore_id(path: Path, ra: int, ca: int):
    txt = path.read_text()
    tokens = tokenize_keep_markers(txt)
    payload_size = ra * ca

    # Collect token chunks separated by TLAST if present, else do fixed-size chunking (1 + payload)
    chunks = []
    cur = []
    saw_tlast = False
    for tok in tokens:
        if tok.upper() == "TLAST":
            if cur:
                chunks.append(cur)
                cur = []
            saw_tlast = True
            continue
        if tok.upper() in {"HEADER"}:
            continue
        try:
            cur.append(try_parse_int(tok))
        except Exception:
            continue

    if cur:
        if saw_tlast:
            # append trailing chunk even if incomplete (we'll check size later)
            chunks.append(cur)
        else:
            # no TLAST: split by fixed pkt_len
            pkt_len = 1 + payload_size
            total = len(cur)
            nfull = total // pkt_len
            for i in range(nfull):
                start = i * pkt_len
                chunks.append(cur[start:start + pkt_len])
            rem = total - nfull * pkt_len
            if rem:
                print(f"Warning: trailing {rem} ints in {path.name} ignored")

    # Convert to payloads: drop first int (ID) and take next payload_size ints
    A_payloads = []
    for idx, c in enumerate(chunks):
        if len(c) < 1 + payload_size:
            print(f"Skipping incomplete packet #{idx} (len {len(c)})")
            continue
        payload = c[1:1 + payload_size]
        A_payloads.append(payload)
    return A_payloads


def read_B_matrices(path: Path, rb: int, cb: int):
    txt = path.read_text()
    tokens = tokenize_keep_markers(txt)
    vals = []
    for tok in tokens:
        if tok.upper() in {"TLAST", "HEADER", "ID"}:
            continue
        try:
            vals.append(try_parse_int(tok))
        except Exception:
            continue
    bsize = rb * cb
    if len(vals) < bsize:
        raise ValueError(f"{path.name} too short: need at least {bsize} ints, got {len(vals)}")
    mats = []
    nmat = len(vals) // bsize
    for i in range(nmat):
        mats.append(vals[i * bsize:(i + 1) * bsize])
    if len(vals) % bsize:
        print(f"Warning: trailing {len(vals) % bsize} ints in {path.name} ignored")
    return mats


# ----------------- core compute -----------------
def compute_col_maxima(A_vals, B_vals, Ra, Ca, Cb):
    # A_vals: length Ra*Ca, B_vals: length Ca*Cb
    col_max = [-1e30] * Cb
    for r in range(Ra):
        row_off = r * Ca
        for j in range(Cb):
            s = 0.0
            for k in range(Ca):
                a = float(A_vals[row_off + k])
                b = float(B_vals[k * Cb + j])
                s += a * b
            if s > col_max[j]:
                col_max[j] = s
    return col_max


def main():
    if not IN0.exists():
        raise FileNotFoundError(f"Missing {IN0}")
    if not IN1.exists():
        raise FileNotFoundError(f"Missing {IN1}")

    Ra, Ca, Rb, Cb = F_Ra, F_Ca, F_Rb, F_Cb

    A_packets = read_A_packets_ignore_id(IN0, Ra, Ca)   # list of A payload lists
    B_mats = read_B_matrices(IN1, Rb, Cb)               # list of B matrix lists

    if not A_packets:
        raise ValueError("No valid A packets found")
    if not B_mats:
        raise ValueError("No valid B matrices found")

    pkt_count = min(len(A_packets), len(B_mats))
    if len(A_packets) != len(B_mats):
        print(f"Warning: A packets ({len(A_packets)}) != B matrices ({len(B_mats)}). Using {pkt_count} packets (min).")

    # Prepare base timestamps per packet
    if base_timestamps:
        # copy and extend if necessary
        bases = base_timestamps[:]
        if len(bases) < pkt_count:
            last = bases[-1] if bases else DEFAULT_START_TS
            bases.extend([last] * (pkt_count - len(bases)))
    else:
        bases = [DEFAULT_START_TS + p * DEFAULT_TS_STEP * Cb for p in range(pkt_count)]
        # note: above makes successive packets start after previous packet's last timestamp
        # (you can adjust logic if you want different spacing)

    with OUT.open("w") as f:
        for p in range(pkt_count):
            A = A_packets[p]
            B = B_mats[p]
            col_max = compute_col_maxima(A, B, Ra, Ca, Cb)

            # emit each column max then a timestamp line
            ts = bases[p]
            for j in range(Cb):
                f.write(f"{int(col_max[j])}\n")
                f.write(f"T {ts} ps\n")
                ts += DEFAULT_TS_STEP

            # TLAST then repeat last value (to match the sample)
            f.write("TLAST\n")
            f.write(f"{int(col_max[-1])}\n")

            # also print to stdout for debugging
            print(f"Packet {p+1} outputs (wrote {Cb} cols + TLAST): last={int(col_max[-1])}")

    print(f"Golden file written to {OUT} (packets: {pkt_count})")


if __name__ == "__main__":
    main()
