# C++ Slotmap Implementation

[![Build Status](https://github.com/nnen/slotmap/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/nnen/slotmap/actions/)
![MIT](https://img.shields.io/badge/license-MIT-blue.svg)

This is a C++ slotmap implementation. For a detailed explanation of what a
slotmap is, see:

 * [Allan Deutsch (2017). C++ Now 2017: "The Slot Map Data Structure".](https://youtu.be/SHaAR7XPtNU?si=6clk4jhFL_sk50lY).
 * [Sean Middleditch (2013). Data Structures for Game Developers: The Slot Map.](https://web.archive.org/web/20180121142549/http://seanmiddleditch.com/data-structures-for-game-developers-the-slot-map/).

## Source

The slotmap implementation is in the `slotmap/slotmap.h` and `slotmap/slotmap.inl` files.

## Benchmarks

There is a Google Benchmark based benchmark in the `slotmap-benchmarks` directory.

## Tests

There are Google Test based tests in the `slotmap-tests` directory.
