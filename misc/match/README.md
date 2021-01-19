Match

```
# Download opening book
wget -P misc/match/data https://github.com/official-stockfish/books/raw/master/8moves_v3.pgn.zip
unzip -d misc/match/data misc/match/data/8moves_v3.pgn.zip

# Build stockfish
make -C thirdparty/Stockfish/src -j build

# Run cutechess-cli
bash misc/match/run.sh
```

Match against specific branch or commit

```
# Build engine from master branch
bash misc/match/prepare.sh master

# Run cutechess-cli
ENGINE2="name=toy-chess-master cmd=build/master/build/Release/main" bash misc/match/run.sh
```

Match against different network

```
ENGINE2="name=toy-chess-ckpt cmd=build/Release/main option.WeightFile=$PWD/src/nn/data/ckpt-2021-01-16-07-11-46.bin" bash misc/match/run.sh
```
