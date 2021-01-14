Match

```
# Download opening book
wget -P misc https://github.com/official-stockfish/books/raw/master/8moves_v3.pgn.zip
unzip -d misc misc/8moves_v3.pgn.zip

# Build stockfish
make -C thirdparty/Stockfish/src -j build

# Run cutechess-cli
bash misc/match.sh
```
