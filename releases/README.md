Firmware release binaries (.uf2) live in this folder, one file per version.

## Process to publish a new version

1. Bump `FW_VERSION_MAJOR` / `FW_VERSION_MINOR` / `FW_VERSION_PATCH` in `src/core/version.h`.
2. Build the combined image (firmware + LittleFS data):
   ```bash
   python3 scripts/build_combined.py
   ```
   This produces `combined.uf2` at the repo root.
3. Copy it into this folder, named `param8-midi-controller-vX.Y.Z.uf2` (must match the version bumped in step 1).
4. Update `manifest.json` at the repo root:
   - `version`: `"X.Y.Z"`
   - `file`: `"releases/param8-midi-controller-vX.Y.Z.uf2"`
   - `notes`: short changelog
   - `date`: release date
5. Commit and push. The editor reads `manifest.json` from this repo's default branch to detect available updates.

Older `.uf2` files can be kept here for rollback, or removed once no longer needed.
