# Limit Order Book

A single-instrument, price-time-priority limit order book implemented in C++17.

## Features

- Limit orders with `GTC` / `IOC` / `FOK` time-in-force
- Market orders
- O(log n) price-level access via `std::map`, O(1) order lookup and O(log n) cancel via a hash-indexed order table
- Cancel and modify (cancel + re-submit) operations
- Price/depth queries: best bid/ask, aggregated depth by level

## Layout

- `include/lob/` — public headers (`Types.hpp`, `OrderBook.hpp`)
- `src/` — implementation
- `tests/` — unit tests (self-contained, no external test framework dependency)
- `examples/` — small CLI demo exercising the book

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run tests

```sh
ctest --test-dir build --output-on-failure
```

## Run the demo

```sh
./build/lob_demo
```

## Live order book (two terminals)

Terminal 1 shows the book and redraws it after every order:

```sh
./build/lob_live
```

Terminal 2 is where you type orders:

```sh
./build/lob_live order
> buy 10 @ 100
> sell 4 @ 102
> sell 6 @ 99 ioc
> market buy 3
> cancel 1
> modify 2 8 @ 101
```

Order ids are assigned automatically and shown in the activity log.
Order types: `gtc` (default), `ioc`, `fok`.
