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

Match against specific commit

```
# Build specific commit
bash misc/match/prepare.sh 5662276

# Run cutechess-cli
ENGINE2="name=toy-chess-5662276 cmd=build/5662276/build/Release/main" bash misc/match/run.sh
```
