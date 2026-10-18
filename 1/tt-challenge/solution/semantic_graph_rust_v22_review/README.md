# Semantic Graph V22 — Rust state-path analysis and parity-ready build

This is the controlled successor to the last confirmed V19 submission (leaderboard **0.908720**). **Predicted edge sets in `submission.json` are intentionally identical to V19.** This edition improves reproducibility and models separate scenario-created orders, but it does **not claim a higher leaderboard score**.

## What changed

- `src/ledger.rs`: maintains individual order states rather than a single global `state`. Tracks `Reserved`, `Paid`, `Cancelled`, `Changed`, and `Collected`, plus refund events.
- S2 state path: first regular order booked → paid → cancelled with refund; second regular order booked → paid → rebooked to a cheaper class, with difference refund.
- S3 state path: two independent high-speed reservations; first and second payments apply separately to the oldest unpaid order.
- `src/main.rs` passes the scenario's `cheaper` predicate to the rebooking state transition.
- Tests check these paths against actual supplied Java source and the warmup SQL dump, without launching Train Ticket.
- A strict Cargo/JSON parity test is included. The environment where this archive was authored had **no `cargo`/`rustc` and no reachable download mirror**: that test is **blocked**, not passed.

## Required final Rust validation (before claiming bonus)

Unzip the original `Semantic Graph.zip` to obtain a `tt-challenge/` folder, then from this submission directory run:

```bash
cargo test --release --locked
cargo run --release --locked -- --base /absolute/path/to/tt-challenge --out regenerated.json
python3 tests/check_submission.py --base /absolute/path/to/tt-challenge --submission submission.json --generated regenerated.json
```

Or run all three automatically:

```bash
python3 tests/rebuild_and_compare.py --base /absolute/path/to/tt-challenge
```

**Windows PowerShell**: install the Rust toolchain (`rustup`) and Python first, extract both archives, then substitute a Windows path like `C:\\Users\\you\\Desktop\\tt-challenge` for `/absolute/path/to/tt-challenge`. The commands otherwise remain the same (use `python` if `python3` is not available).

A successful offline JSON validation is **not** a Rust build or regeneration proof. If Rust regeneration disagrees with the shipped `submission.json`, do not claim the Rust bonus until that mismatch is resolved.

## Non-Rust checks performed

```bash
python3 tests/check_s2_s3_java.py --base /absolute/path/to/tt-challenge
python3 tests/check_submission.py --base /absolute/path/to/tt-challenge --submission submission.json
python3 /absolute/path/to/tt-challenge/platform/evaluate_golden.py /absolute/path/to/tt-challenge/gold/S0.json submission.json
```

The public evaluator checks S0 correctness and only validates the S1–S7 **format**. Only the leaderboard can supply hidden scenario scores. No user-facing score improvement is asserted for V22.
