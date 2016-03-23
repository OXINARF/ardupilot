#!/usr/bin/env bash
# Install dependencies and configure the environment for CI build testing

set -ex

if [ "$OS_NAME" = 'osx' ]; then
    Tools/scripts/configure-ci-osx.sh
elif [ "$OS_NAME" = 'linux' ] || [ -z "$OS_NAME" ]; then
    Tools/scripts/configure-ci-linux.sh
fi
