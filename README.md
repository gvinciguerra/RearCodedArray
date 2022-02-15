# RearCodedArray

Implementation of a simple compressed sorted string dictionary based on **rear-coding** and **fixed-length blocks**.
It supports the **select** (aka access) operation, which returns the _i_-th lexicographically-ranked string for a given integer _i_, and the **rank** (aka lexicographic search) operation, which counts the number of strings that are lexicographically smaller than or equal to a given string pattern.

Rear-coding compresses the sorted strings in the dictionary by removing the longest common prefix, of length _lcp_, between any two consecutive strings _s_, _t_, and by encoding the length of the suffix of _s_ to be removed from it to get the value _lcp_.  This length is encoded via the variable-byte code.

To provide fast query operations, a bucketing approach is used: when a given amount of bytes (`block_bytes`) is written, the compression is "restarted" by creating a block in which the next string to be encoded (referred to as _header_) is stored explicitly. The cumulative counts of the strings in the blocks are also stored. Then, select (resp. rank) is implemented via a binary search on these counts (resp. headers) to find the appropriate block containing the answer, followed by a sequential decompression of the block. 

Two implementations are provided: one that stores the headers at the start of the blocks, the other that stores the headers separately in a contiguous area.

## Usage

This is a header-only library. To compile the [example](example.cpp), use the following commands:

```sh
git clone https://github.com/gvinciguerra/RearCodedArray.git
cd RearCodedArray
cmake . -DCMAKE_BUILD_TYPE=Release
make
```

## References

1. Paolo Ferragina, Roberto Grossi, Ankur Gupta, Rahul Shah, Jeffrey Scott Vitter. [On searching compressed string collections cache-obliviously](https://doi.org/10.1145/1376916.1376943). PODS 2008: 181-190.
2. Miguel A. Martínez-Prieto, Nieves R. Brisaboa, Rodrigo Cánovas, Francisco Claude, Gonzalo Navarro. [Practical compressed string dictionaries](https://doi.org/10.1016/j.is.2015.08.008). Inf. Syst. 56: 73-108 (2016).