import numpy as np

def read_vector(filename):
    """Read int32 vector from file."""
    return np.loadtxt(filename, dtype=np.int32)

def dot_product(vec1, vec2):
    """Compute dot product using int32 arithmetic."""
    if vec1.shape != vec2.shape:
        raise ValueError("Vectors must be the same length")
    # Multiply and sum in int32, then cast to int32 to avoid overflow
    return np.int32(np.sum(np.int32(vec1) * np.int32(vec2)))

def main():
    vector0 = read_vector("input0.txt")
    vector1 = read_vector("input1.txt")

    result = dot_product(vector0, vector1)
    print("Dot product:", result)

if __name__ == "__main__":
    main()
