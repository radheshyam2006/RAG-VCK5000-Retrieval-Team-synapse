import numpy as np

# this is for inner product code 

SEQ_ROWS = 128
SEQ_COLS = 32

TXT_ROWS = SEQ_COLS
TXT_COLS = 32

def gen_input0_txt(path, seed=0):
	"""Generate plain text file for one 32x32 matrix, row-major, one value per line."""
	rng = np.random.default_rng(seed)
	A = rng.integers(low=0, high=10, size=(TXT_ROWS * TXT_COLS), endpoint=False)
	with open(path, 'w') as f:
		for v in A:
			f.write(f"{int(v)}\n")

def gen_input1_seq(path, num_packets=6, seed=1):
	"""
	Generate pktstream sequence file containing 3 matrices of shape 128x32.
	Format per packet: header, (SEQ_ROWS*SEQ_COLS - 1) payloads, TLAST, final payload.
	"""
	rng = np.random.default_rng(seed)
	with open(path, 'w') as f:
		for p in range(num_packets):
			header = 2415853568 + p  # arbitrary header IDs per packet
			f.write(f"{header}\n")
			A = rng.integers(low=0, high=10, size=(SEQ_ROWS * SEQ_COLS), endpoint=False)
			for v in A[:-1]:
				f.write(f"{int(v)}\n")
			f.write("TLAST\n")
			f.write(f"{int(A[-1])}\n")

def gen_input0_seq(path, num_packets=6, seed=2):
	"""
	Generate pktstream sequence file containing 3 matrices of shape 128x32.
	Format per packet: header, (SEQ_ROWS*SEQ_COLS - 1) payloads, TLAST, final payload.
	"""
	rng = np.random.default_rng(seed)
	with open(path, 'w') as f:
		for p in range(num_packets):
			header = 3415853568 + p  # distinct header base for input0.seq
			f.write(f"{header}\n")
			A = rng.integers(low=0, high=10, size=(SEQ_ROWS * SEQ_COLS), endpoint=False)
			for v in A[:-1]:
				f.write(f"{int(v)}\n")
			f.write("TLAST\n")
			f.write(f"{int(A[-1])}\n")

def gen_input1_txt(path, seed=3):
	"""Generate plain text file for one 32x32 matrix, row-major, one value per line."""
	rng = np.random.default_rng(seed)
	B = rng.integers(low=1, high=5, size=(TXT_ROWS * TXT_COLS), endpoint=False)
	with open(path, 'w') as f:
		for v in B:
			f.write(f"{int(v)}\n")

if __name__ == "__main__":
	gen_input0_seq("data/input0.seq", num_packets=6)
	gen_input1_txt("data/input1.txt")
