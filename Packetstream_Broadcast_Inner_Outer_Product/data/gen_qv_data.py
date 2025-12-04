import numpy as np
from pathlib import Path

# this is for the outerproduct data flow 

Ra = 4   
Ca = 16  
Rb = 16  
Cb = 4   

out_dir = Path(__file__).parent
INPUT0_SEQ = out_dir / "input0.seq"   # pktstream sequence (pktsplit to 3 kernels)
INPUT1_TXT = out_dir / "input1.txt"   # broadcast stream payload (shared across 3 kernels)

# RNG seeds for reproducibility
SEED_Q = 100
SEED_V = 200


def generate_Q_broadcast_columns(path: Path, seed: int = SEED_Q):
    """
    Generate broadcast stream payload for Q:
    Order: for k = 0..Ca-1, write 32 int values representing Q[0..31, k].
    No headers; plain text, one value per line.
    """
    rng = np.random.default_rng(seed)
    # Q as integers to match kernel's int32 input, range 0..9
    Q = rng.integers(low=0, high=10, size=(Ra, Ca), endpoint=False, dtype=np.int32)
    with path.open("w") as f:
        for k in range(Ca):
            for r in range(Ra):
                f.write(f"{int(Q[r, k])}\n")


def generate_V_packets_seq(path: Path, num_packets: int = 3, seed: int = SEED_V, header_base: int = 0xCAFEB000):
    """
    Generate pktstream sequence file for V rows:
    Format: single packet -> header, then (Rb*Cb - 1) payloads, TLAST, final payload.
    Order: for k = 0..Rb-1, write 32 int values representing V[k, 0..31].
    """
    with path.open("w") as f:
        for p in range(num_packets):
            rng = np.random.default_rng(seed + p)
            V = rng.integers(low=0, high=10, size=(Rb, Cb), endpoint=False, dtype=np.int32)
            header = header_base + p
            f.write(f"{header}\n")
            # Write all but last payload for this packet
            total = Rb * Cb
            count = 0
            for k in range(Rb):
                for j in range(Cb):
                    if count < total - 1:
                        f.write(f"{int(V[k, j])}\n")
                    count += 1
            # TLAST then final payload value for this packet
            f.write("TLAST\n")
            f.write(f"{int(V[Rb-1, Cb-1])}\n")


if __name__ == "__main__":

    generate_Q_broadcast_columns(INPUT1_TXT)
    generate_V_packets_seq(INPUT0_SEQ, num_packets=3)
    print(f"Wrote: {INPUT1_TXT}")
    print(f"Wrote: {INPUT0_SEQ}")
