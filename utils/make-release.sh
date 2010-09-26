#!/bin/sh

[ $# -eq 0 ] && echo "Usage: $0 release_version" && exit 0

git archive -v --format=tar --prefix=pokanalysis-$1/ HEAD | gzip --best > www/releases/pokanalysis-$1.tar.gz
