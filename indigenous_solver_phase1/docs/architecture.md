# Architecture

File input -> parser -> LinearModel -> validation -> sparse matrix -> solver -> SolveResult -> CLI.

The solution is intended to be independently checked in later phases. Solver algorithms do not own file parsing, and model storage is independent of simplex internals.
