#!/bin/bash

TIMESTAMP=$(date '+%F-%H-%M-%S')

CLI=${CLI:-"cutechess-cli"}

CONCURRENCY=${CONCURRENCY:-2}
ROUNDS=${ROUNDS:-20}
TC=${TC:-"10+0.1"}
DRAW=${DRAW:-"-draw movenumber=40 movecount=4 score=10"}
RESIGN=${RESIGN:-"-resign movecount=4 score=1000"}
OPENINGS=${OPENINGS:-"-openings file=misc/match/data/8moves_v3.pgn policy=round -repeat"}

RESULT=${RESULT:-"misc/match/data/result-$TIMESTAMP.pgn"}
LOG=${LOG:-"misc/match/data/log-$TIMESTAMP.txt"}

SF_ELO=${SF_ELO:-1700}

ENGINE1=${ENGINE1:-"name=toy-chess cmd=build/Release/main"}
ENGINE2=${ENGINE2:-"name=stockfish cmd=stockfish dir=thirdparty/Stockfish/src option.UCI_LimitStrength=true option.UCI_Elo=$SF_ELO"}

set -x
$CLI \
  -tournament round-robin \
  -concurrency $CONCURRENCY \
  -engine tc=$TC $ENGINE1 -engine tc=$TC $ENGINE2 -each proto=uci \
  -rounds $ROUNDS -games 2 \
  -pgnout $RESULT \
  -debug \
  $OPENINGS $DRAW $RESIGN |& tee $LOG | grep -v -P '^\d'
