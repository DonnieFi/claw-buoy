# Public release checklist

## Release boundary

Publish a **fresh, allowlisted source export**, not this device checkout or its existing Git history. `tools/public_export.py` copies source, portable configuration/defaults, the dependency lock, host tests, licenses, this guide, root docs, CI, and the v5 enclosure. It excludes local deployment guides/evidence, all release archives/backups, live caches, credentials, binaries, and Git metadata. The exact file set and SHA-256 hashes are written to the candidate's `PUBLIC_MANIFEST.json`.

The original working tree, history, private documents, and rollback artifacts stay intact. `.gitignore` prevents common accidental adds; it **does not remove already-tracked data** and is not the publication boundary.

## Audit outcome (2026-10-15)

Neither Gitleaks nor TruffleHog was initially installed. Gitleaks v8.24.3 was then built into a temporary tools directory and scanned all 3 existing commits (about 832 KB of Git diff content), with full redaction: **no leaks found**. This is a secret-pattern result, not a privacy clearance. Separately, a dependency-free, redacted heuristic triage scanned **86 tracked files, 116 unique reachable history blobs, and 3 commits**. It found no match for its credential/token/private-key rules, but found **11 privacy/example-network matches**: two installation-specific paths in current UI source, two upstream private-IP documentation examples, and seven history matches including local-host references in earlier deployment notes. Findings overlap across current and historical copies. This is **not proof of no secrets**: encoded assets, arbitrary personal names, unknown token formats, images, and untracked private artifacts are not comprehensively checked.

Do not publish the existing history. Installation-specific action prompts and delivery targets now live in an ignored private override header excluded by the export allowlist; public defaults use generic local-only actions. Example upstream private-IP documentation is omitted from the candidate rather than misrepresented as a leaked credential. A fresh export is scanned before it is created; failures report category/path/line only, never matching values. Local audit detail is intentionally not included in the public candidate.

## Prepare and verify locally

1. Finish the UI work in the working source. Preserve GPIO17 backlight, calibration/settings, alarms, and first-tap wake behavior. Do not claim V8 device verification before it occurs.
2. Run `python3 tools/public_checks.py`.
3. Choose a **new directory outside this checkout**, then run `python3 tools/public_export.py /path/to/new-public-candidate`. Never export over an existing tree.
4. In the candidate, review `PUBLIC_MANIFEST.json`, all text/configuration defaults and binary assets. Confirm that sample content is synthetic. The export is intentionally an allowlist, not a mirror.
5. Perform a clean ESP-IDF 5.4.2 build from the candidate using the root README command. The lock records registry dependency resolution; the candidate deliberately contains no build cache or managed components.
6. Run a maintained secret scanner against the candidate before publication when available, plus human privacy review of source strings, artwork, docs, licenses, and author metadata. Heuristic triage is not a substitute. A history audit cannot clear unrelated local untracked files for release.
7. Confirm branding/artwork provenance and redistribution rights. Retain Apache-2.0 upstream notices and jsmn's MIT notice. Root project MIT must not overwrite upstream licenses.
8. Confirm a public repository name/owner and whether to enable private vulnerability reporting. Use intentional public author identity for the new initial commit.
9. Only after review, initialize a **new Git repository in the candidate**, review staged paths/diff, and create its first commit. Do not attach or push the old `.git` directory.
10. Create/push the public GitHub repository only when authorized. No remote creation, upload, push, visibility change, or history rewrite is performed by these preparation tools.

## Status language

- **V7 physically verified:** GPIO17 PWM wiring, idle backlight darkness; touch wake works but can require multiple taps.
- **V8 built and deployed:** host tests, USB flash with hash verification, node/sensor response and no-send live UI regression passed. Calibration, alarm and cached content restored. Actual LCD appearance, touch feedback feel and V8 physical idle/wake remain user checks. No test messages were sent.
- **Enclosure:** geometric checks only; no confirmed print fit or successful slice verification.
- **CI:** the workflow is prepared locally. GitHub execution remains unverified until a repository is published and the workflow actually runs.

## Remaining release decisions

- Choose public repository owner/name and intentional commit author identity.
- Clean candidate build and Gitleaks scan passed locally; repeat the scan on the final staged export. Review copyright author identity and mascot redistribution rights before publication.
- Confirm redistribution rights for mascot/artwork; use a synthetic device photo if one is added.
- Confirm V8 physical UX behavior on the deployed device; retain honest prototype/enclosure limitations.
