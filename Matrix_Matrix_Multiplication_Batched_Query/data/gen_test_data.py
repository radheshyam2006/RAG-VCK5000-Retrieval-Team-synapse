# Copyright (C) 2023 Advanced Micro Devices, Inc
#
# SPDX-License-Identifier: MIT

import numpy as np
import struct
import re
from pathlib import Path
from typing import Dict, Tuple, Optional
from write_file import mat2file_tile

shape_a = (128, 32)
shape_b = (32, 32)

DATA_DIR = Path(__file__).parent
REPO_ROOT = DATA_DIR.parent
HEADER_CANDIDATES = [
    REPO_ROOT / "build.hw" / "work" / "temp" / "packet_ids_c.h",
    REPO_ROOT / "build.x86sim" / "work" / "temp" / "packet_ids_c.h",
    REPO_ROOT / "build" / "work" / "temp" / "packet_ids_c.h",
]
PACKET_TYPE = 0


def mat2file_full(mat, filename):
    """Write matrix exactly in row-major 2-D format."""
    rows, cols = mat.shape
    with open(filename, "w") as f:
        for r in range(rows):
            line = " ".join(str(mat[r, c]) for c in range(cols))
            f.write(line + "\n")


def locate_packet_header() -> Optional[Path]:
    for candidate in HEADER_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def parse_packet_ids(header_path: Optional[Path]) -> Dict[str, int]:
    if header_path is None:
        return {}
    pattern = re.compile(r"#define\s+(\w+)\s+(\d+)")
    mapping: Dict[str, int] = {}
    with open(header_path, "r", encoding="utf-8") as f:
        for line in f:
            match = pattern.match(line.strip())
            if match:
                mapping[match.group(1)] = int(match.group(2))
    return mapping


def float_to_u32(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", np.float32(value)))[0]


def build_header(pkt_type: int, pkt_id: int) -> int:
    header = 0
    header |= pkt_id & 0x1F                   # bits [4:0]
    header |= (pkt_type & 0x7) << 12          # bits [14:12]
    header |= 0x1F << 16                      # source row = -1 -> bits [20:16]
    header |= 0x7F << 21                      # source col = -1 -> bits [27:21]
    header |= 0 << 28                         # bits [30:28]
    lower = header & 0x7FFFFFFF
    parity = bin(lower).count("1") & 0x1      # xor-reduce bits[30:0]
    if parity == 0:
        header |= 1 << 31                     # make total count odd
    else:
        header &= ~(1 << 31)
    return header


def write_packet_file(words, header: int, filename: str):
    pkt_path = DATA_DIR / filename
    with open(pkt_path, "w", encoding="utf-8") as f:
        f.write(f"{header}\n")
        for idx, word in enumerate(words):
            if idx == len(words) - 1:
                f.write("TLAST\n")
            f.write(f"{word}\n")


def emit_packetized_inputs(a: np.ndarray, b: np.ndarray):
    header_path = locate_packet_header()
    packet_ids = parse_packet_ids(header_path)
    if header_path is None:
        print(
            "WARNING: packet_ids_c.h not found. "
            "Generated packet files will default to ID 0. "
            "Run `make aie` first to populate accurate IDs."
        )

    packets: Tuple[Tuple[str, np.ndarray, str], ...] = (
        ("DataInFP_A_0", a, "inputa_packets.txt"),
        ("DataInFP_B_0", b, "inputb_packets.txt"),
    )
    for macro_name, matrix, filename in packets:
        pkt_id = packet_ids.get(macro_name, 0)
        header = build_header(PACKET_TYPE, pkt_id)
        words = [float_to_u32(v) for v in matrix.flatten()]
        write_packet_file(words, header, filename)


def main():
    """Generate random matrices for different data types and write to files"""
    np.random.seed(12262023)
    # float
    c0 = np.random.randint(2**4, 2**8)
    c1 = np.random.randint(2**6, 2**9)
    a = np.float32(np.trunc(np.random.rand(*shape_a) * c0))
    b = np.float32(np.trunc(np.random.rand(*shape_b) * c1))
    c = np.matmul(a, b)
    mat2file_tile(a, 4, 2, "inputa_float.txt")
    mat2file_tile(b, 2, 4, "inputb_float.txt")
    mat2file_tile(c, 4, 4, "ref_outputc_float.txt", sn=True)

    mat2file_full(c, "outputc_full_float.txt")
    emit_packetized_inputs(a, b)

    # # int32
    # a = np.random.randint(-1*2**12, 2**12, size=(shape_a), dtype=np.int32)
    # b = np.random.randint(-1*2**12, 2**12, size=(shape_b), dtype=np.int32)
    # c = np.matmul(a, b)
    # mat2file_tile(a, 4, 2, "inputa_int32.txt")
    # mat2file_tile(b, 2, 4, "inputb_int32.txt")
    # mat2file_tile(c, 4, 4, "ref_outputc_int32.txt")

    # # 16-bit
    # a = np.random.randint(-1*2**5, 2**5, size=(shape_a), dtype=np.int16)
    # b = np.random.randint(-1*2**5, 2**5, size=(shape_b), dtype=np.int16)
    # c = np.matmul(a, b)
    # mat2file_tile(a, 4, 4, "inputa_int16.txt")
    # mat2file_tile(b, 4, 8, "inputb_int16.txt")
    # mat2file_tile(c, 4, 8, "ref_outputc_int16.txt")

    # # 8-bit
    # a = np.random.randint(-1*2**4, 2**6, size=(shape_a), dtype=np.int8)
    # b = np.random.randint(-1*2**4, 2**6, size=(shape_b), dtype=np.int8)
    # c = np.matmul(a, b)
    # mat2file_tile(a, 4, 8, "inputa_int8.txt")
    # mat2file_tile(b, 8, 4, "inputb_int8.txt")
    # mat2file_tile(c, 4, 4, "ref_outputc_int8.txt")

    # # Mixed precision
    # # A is 16-bit and B is 8-bit
    # """
    # a = np.random.randint(-1*2**5, 2**5, size=(shape_a), dtype=np.int16)
    # b = np.random.randint(-1*2**4, 2**6, size=(shape_b), dtype=np.int8)
    # c = np.matmul(a, b)
    # mat2file_tile(a,<Tile R>,<Tile C>, "inputa_int_8_16.txt")
    # mat2file_tile(b,<Tile R>,<Tile C>, "inputb_int_8_16.txt")
    # mat2file_tile(c,<Tile R>,<Tile C>, "ref_outputc_int_8_16.txt")
    # """


if __name__ == '__main__':
    main()
