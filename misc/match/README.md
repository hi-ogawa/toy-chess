Match against elo-emulated Stockfish

```
# Download opening book
wget -P misc/match/data https://github.com/official-stockfish/books/raw/master/noob_3moves.epd.zip
unzip -d misc/match/data misc/match/data/noob_3moves.epd.zip

# Build stockfish
make -C thirdparty/Stockfish/src -j build ARCH=x86-64-avx2

# Run cutechess-cli
bash misc/match/run.sh
```

Match against itself

```
# Enable debug info to help reading coredump
python script.py init --clang --b RelWithDebInfo
python script.py build --b RelWithDebInfo

# Run cutechess-cli (and wait for crush...)
ENGINE1="name=toy-chess-1 cmd=build/RelWithDebInfo/main" ENGINE2="name=toy-chess-2 cmd=build/RelWithDebInfo/main" bash misc/match/run.sh

# Read coredump
coredumpctl --debugger=lldb debug
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
