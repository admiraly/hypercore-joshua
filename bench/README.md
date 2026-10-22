# Performance corpus: M0 foundation

No workload benchmarks or superiority claims exist yet. Boot smoke tests measure
correctness only. QEMU TCG timings must not be compared with native Linux timings.

`results.schema.json` defines the initial record contract. Each workload record
must include at least three independent exploratory trials, security and durability
settings, exact platform/toolchain identities, workload seed, and units. Release
claims additionally need a justified sample size, confidence intervals, warm/cold
separation, frequency/temperature recording, and matched tuned-Linux measurements.

Until native clocks and workload harnesses exist, do not populate this corpus with
invented timings, emulator boot duration, or hashes masquerading as performance.
