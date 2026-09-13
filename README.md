# The AIMer Signature scheme
This repository contains the source codes of the AIMer signature scheme.
The source codes comply with the Korean Industrial Standards (KS) specification.

## Introduction
AIMer is a digital signature scheme derived from a zero-knowledge proof of preimage knowledge for a symmetric primitive.
AIMer is a final algorithm of the KpqC competition.

https://kpqc.or.kr/contents/03_exhibit/sub_03.html

## Folder Structure
* `KAT`: the known answer test files for each parameter set of the AIMer scheme
* `Reference_Implementation`: the reference implementation.

## Parameter Sets
A single implementation supports all six parameter sets, selected at build time
via `PARAMS`: `128f`, `128s`, `192f`, `192s`, `256f`, `256s` (NIST security
levels 1/3/5, each in a fast `f` and small `s` variant).

## Build and Test
The reference implementation is built with `make` (GCC or Clang):

```
cd Reference_Implementation
make           # build test_sign and PQCgenKAT_sign for all parameter sets
make check     # regenerate KATs and diff them against ../KAT
make clean     # remove build artifacts
```

Executables are placed under `build/<param>/`. `make check` regenerates each
parameter set's response file and compares it against the committed
known-answer test in `KAT/`.

## Platform
This reference implementation targets little-endian platforms. A build
on a known big-endian target fails with a compile-time error.

## Authors
Seongkwang Kim, Jihoon Cho, Jincheol Ha, Jihoon Kwon, Byeonghak Lee, Joohee Lee,
Jooyoung Lee, Sangyub Lee, Dukjae Moon, Mincheol Son, Hyojin Yoon.

## Homepage
More information about AIMer is available at:

https://aimer-signature.org
