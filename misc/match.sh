#!/bin/bash

TIMESTAMP=$(date '+%F-%H-%M-%S')

CLI=${CLI:-"cutechess-cli"}

ROUNDS=${ROUNDS:-5}
TC=${TC:-"1/1+0.0"}
DRAW=${DRAW:-"-draw movenumber=40 movecount=4 score=10"}
RESIGN=${RESIGN:-"-resign movecount=4 score=1000"}
RESULT=${RESULT:-"-pgnout misc/match-$TIMESTAMP.pgn"}
OPENINGS=${OPENINGS:-"-openings file=misc/8moves_v3.pgn policy=round -repeat"}

SF_ELO=${SF_ELO:-1500}

ENGINE1=${ENGINE1:-"tc=$TC name=toy-chess cmd=build/Release/main"}
ENGINE2=${ENGINE2:-"tc=$TC name=stockfish cmd=stockfish dir=thirdparty/Stockfish/src option.UCI_LimitStrength=true option.UCI_Elo=$SF_ELO"}

set -x
$CLI \
  -tournament round-robin \
  -concurrency 2 \
  -engine $ENGINE1 -engine $ENGINE2 -each proto=uci \
  -rounds $ROUNDS -games 2 \
  $OPENINGS $DRAW $RESIGN $RESULT
