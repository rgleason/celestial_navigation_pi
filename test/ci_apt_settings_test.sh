#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/../ci/apt-ci-settings.sh"
# Pure checks only: never call apt_ci_prepare/sudo or modify host apt settings.
settings=$(apt_ci_configuration)
[[ "$settings" == *'Acquire::Retries "5";'* ]]
[[ "$settings" == *'Acquire::http::Timeout "30";'* ]]
[[ "$settings" == *'Acquire::https::Timeout "30";'* ]]
[[ "$settings" != *'AllowUnauthenticated'* ]]
[[ "$settings" != *'Verify-Peer'* ]]
for source_url in http://archive.ubuntu.com/ubuntu http://us.archive.ubuntu.com/ubuntu; do
  actual=$(printf 'deb %s jammy main\n' "$source_url" | sed -E "$APT_CI_UBUNTU_REWRITE")
  [[ "$actual" == 'deb https://archive.ubuntu.com/ubuntu jammy main' ]]
done
actual=$(printf '%s\n' 'URIs: http://security.ubuntu.com/ubuntu' | sed -E "$APT_CI_UBUNTU_REWRITE")
[[ "$actual" == 'URIs: https://security.ubuntu.com/ubuntu' ]]
for source_line in 'deb https://ppa.launchpadcontent.net/opencpn/opencpn/ubuntu/ jammy main' 'deb http://deb.debian.org/debian bookworm main'; do
  [[ "$(printf '%s\n' "$source_line" | sed -E "$APT_CI_UBUNTU_REWRITE")" == "$source_line" ]]
done
printf '%s\n' 'CI apt transport/retry checks passed (no host changes).'
