# SALAM Table 2 Validation (moved)

This document has moved to **`util/SALAM-docs/validation/`**.

| New location | Purpose |
|--------------|---------|
| [validation/TABLE2_RESULTS.md](../../SALAM-docs/validation/TABLE2_RESULTS.md) | Final PPA error % table (auto-generated) |
| [validation/IMPLEMENTATION.md](../../SALAM-docs/validation/IMPLEMENTATION.md) | Implementation details and metric definitions |
| [validation/PROGRESS.md](../../SALAM-docs/validation/PROGRESS.md) | Timeline and open items |
| [validation/README.md](../../SALAM-docs/validation/README.md) | Index |

Regenerate results:

```bash
python3 util/SALAM-tools/generate_validation_report.py --m5-path /path/to/gem5
```

The June 2026 checkpoint content from this file is preserved in
`IMPLEMENTATION.md` and `PROGRESS.md` (updated through July 2026).
