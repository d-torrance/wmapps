#!/bin/sh
set -e
exec autoreconf --force --install --verbose "$@"
