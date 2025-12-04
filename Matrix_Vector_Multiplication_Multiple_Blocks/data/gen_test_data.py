import numpy as np

def write_file_one_per_line(file: str, data):
    """
    Writes all floats to a file, one element per line.
    Works for 1D or 2D arrays.
    """
    with open(file, 'w') as f:
        if data.ndim == 1:
            for v in data:
                f.write(f"{v:.9e}\n")
        else:
            for row in data:
                for v in row:
                    f.write(f"{v:.9e}\n")

def main():
    VECTOR_SIZE = 32
    NUM_VECTORS = 256  # 🔸 now 512 target vectors

    # Generate query vector
    query = np.random.randn(VECTOR_SIZE).astype(np.float32)
    query /= np.linalg.norm(query)  # normalize

    # Generate 512 target vectors
    targets = np.random.randn(NUM_VECTORS, VECTOR_SIZE).astype(np.float32)
    targets = np.array([v / np.linalg.norm(v) for v in targets], dtype=np.float32)

    # Compute dot products
    dot_products = targets @ query

    # Find max dot product and its index
    max_dot = np.max(dot_products)
    max_index = np.argmax(dot_products)  # index of the target vector giving max dot

    # Print index of max dot product
    print(f"Index of target vector with max dot product: {max_index}")

    # Write query and targets to files
    write_file_one_per_line('input0.txt', query)
    write_file_one_per_line('input1.txt', targets)

    # Write expected max dot product to golden file
    with open('golden.txt', 'w') as f:
        f.write(f"{max_dot:.9e}\n")
        f.write(f"{max_index:.9e}\n")  # scientific notation

    print(f"Max dot product: {max_dot:.9e}")

if __name__ == "__main__":
    main()
