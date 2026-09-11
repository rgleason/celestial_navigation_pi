#!/usr/bin/env bash
# Shared dependency-download settings for disposable Debian/Android CI images.
# Keep signature verification intact; only the transport/retry policy changes.
APT_CI_UBUNTU_REWRITE='s#http://([a-z]{2}\.)?archive\.ubuntu\.com/ubuntu#https://archive.ubuntu.com/ubuntu#g; s#http://security\.ubuntu\.com/ubuntu#https://security.ubuntu.com/ubuntu#g'

apt_ci_configuration() {
  printf '%s\n' \
    'Acquire::Retries "5";' \
    'Acquire::http::Timeout "30";' \
    'Acquire::https::Timeout "30";' \
    'Acquire::http::No-Cache "true";' \
    'Acquire::https::No-Cache "true";'
}

apt_ci_prepare() {
  apt_ci_configuration | sudo tee /etc/apt/apt.conf.d/80-ci-retries
  # Ubuntu HTTP endpoints intermittently stalled in hosted jobs. Use the
  # official TLS endpoints, including country-mirror source-index entries.
  for apt_ci_source in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
    [ -f "$apt_ci_source" ] || continue
    sudo sed -i -E "$APT_CI_UBUNTU_REWRITE" "$apt_ci_source"
  done
}
