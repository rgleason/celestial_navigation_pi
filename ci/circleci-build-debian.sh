#!/usr/bin/env bash

#
# Build the Debian artifacts
#
set -xe

# Hosted mirrors occasionally reset long dependency downloads. Let apt retry
# these transient transfers instead of failing an otherwise healthy target.
echo 'Acquire::Retries "5";' | sudo tee /etc/apt/apt.conf.d/80-ci-retries

if [ "${CIRCLECI_LOCAL,,}" = "true" ]; then
    if [[ -d ~/circleci-cache ]]; then
        if [[ -f ~/circleci-cache/apt-proxy ]]; then
            cat ~/circleci-cache/apt-proxy | sudo tee -a /etc/apt/apt.conf.d/00aptproxy
            cat /etc/apt/apt.conf.d/00aptproxy
        fi
    fi
fi

sudo apt-get -qq update
sudo apt-get install devscripts equivs

# Install extra build libs
ME=$(echo ${0##*/} | sed 's/\.sh//g')
EXTRA_LIBS=./ci/extras/extra_libs.txt
if test -f "$EXTRA_LIBS"; then
    while read -r line; do
        sudo apt-get install $line
    done < "$EXTRA_LIBS"
fi
EXTRA_LIBS=./ci/extras/${ME}_extra_libs.txt
if test -f "$EXTRA_LIBS"; then
    while read -r line; do
        sudo apt-get install $line
    done < "$EXTRA_LIBS"
fi

pwd

git submodule update --init opencpn-libs

sudo mk-build-deps --install ./ci/control

sudo apt-get --allow-unauthenticated install ./*all.deb  || :
sudo apt-get --allow-unauthenticated install -f
rm -f ./*all.deb

TEST_CMAKE_ARGS=""
if [ "${RUN_DATA_TESTS:-false}" = "true" ]; then
  sudo apt-get install -y libgtest-dev
  ci/fetch-eclipse-data.sh eclipse/data --all
  TEST_CMAKE_ARGS="-DOCPN_BUILD_TEST=ON"
fi


if [ -n "$BUILD_GTK3" ] && [ "$BUILD_GTK3" = "TRUE" ]; then
  sudo update-alternatives --set wx-config /usr/lib/*-linux-*/wx/config/gtk3-unicode-3.0
fi

rm -rf build && mkdir build && cd build

tag=$(git tag --contains HEAD)
current_branch=$(git branch --show-current)

if [ -n "$tag" ] || [ "$current_branch" = "master" ]; then
  cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ${TEST_CMAKE_ARGS} ..
else
  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local ${TEST_CMAKE_ARGS} ..
fi

make -j2
make package
if [ "${RUN_DATA_TESTS:-false}" = "true" ]; then
  ctest --output-on-failure
  cd ..
  cmake -S eclipse -B build-eclipse-ci -DCMAKE_BUILD_TYPE=Release
  cmake --build build-eclipse-ci --parallel 2
  ECLIPSE_DE440_PATH="$PWD/eclipse/data/de440s.bsp" \
  ECLIPSE_LUNAR_PCK_PATH="$PWD/eclipse/data/moon_pa_de440_200625.bpc" \
  ECLIPSE_LOLA_PATH="$PWD/eclipse/data/lola64-pa.bin" \
    ctest --test-dir build-eclipse-ci --output-on-failure
  cd build
fi
ls -l
