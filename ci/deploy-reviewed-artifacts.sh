#!/usr/bin/env bash
# Upload only the complete, schema-checked publication reviewed before approval.
set -euo pipefail
set +x
: "${CLOUDSMITH_API_KEY:?CLOUDSMITH_API_KEY is not configured}"
: "${CELESTIAL_CLOUDSMITH_REPO:?Set the approved Cloudsmith owner/repository}"
python3 ci/stage-publication.py publish --repository "$CELESTIAL_CLOUDSMITH_REPO"
