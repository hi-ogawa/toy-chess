#!/bin/bash

COMMIT=$1
[ -z $COMMIT ] && echo ":: COMMIT argument is required." && exit 1

DIR=build/$COMMIT

if [ ! -d $DIR ]; then
  mkdir -p $DIR
  git clone git@github.com:hi-ogawa/toy-chess.git $DIR || exit 1
fi

cd $DIR
git reset --hard $COMMIT
git submodule update --init --depth 1 --progress
python script.py init --clang
python script.py build --t main
