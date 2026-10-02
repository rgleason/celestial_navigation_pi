#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/apt-ci-settings.sh"
apt_ci_prepare
sudo apt-get -q update
sudo apt-get -y install git cmake gettext unzip curl python3 build-essential
exec bash "$(dirname "${BASH_SOURCE[0]}")/build-android.sh" arm64
