Basic Workflow

```
# Generate dataset
SIZE=100000000 DATA_DIR=src/nn/data bash src/nn/training/script.sh gensfen

# Merge .binpack files
cat src/nn/data/gensfen-*.binpack > src/nn/data/gensfen.binpack

# Convert .binpack to .halfkp
./build/Release/nn_preprocess --infile src/nn/data/gensfen.binpack

# Training
python src/nn/training/main.py \
  --dataset=src/nn/data/gensfen.halfkp \
  --test-dataset=src/nn/data/gensfen-test.halfkp \
  --checkpoint-dir=src/nn/data \
  --loss-mode=mse \
  --command=train

# Convert model pramaters to binary for c++ runtime
python src/nn/training/main.py \
  --checkpoint=src/nn/data/ckpt.pt \
  --weight-file=src/nn/data/ckpt.bin \
  --command=process_model_parameters
```

Training on Google Colab

- Colab notebook: https://colab.research.google.com/drive/1r01G0vpKv9HBA06uOMk9RoRydihCpoUi
- Google drive : https://drive.google.com/drive/folders/19Sd_iP8t0_r_XkIu-ibfJ7yzY9M3rkLi?usp=sharing

```
# Use google drive to download dataset (.binpack) and save checkpoints (.pt)
from google.colab import drive
drive.mount('/content/gdrive', force_remount=True)
%env DATA_DIR=/content/gdrive/MyDrive/toy-chess-data
%env LOCAL_DATA_DIR=/content/toy-chess-data
!mkdir -p $LOCAL_DATA_DIR

# Download source code
%cd /content
!rm -rf toy-chess
!git clone https://github.com/hi-ogawa/toy-chess.git
%cd /content/toy-chess

# Build "nn_preprocess"
!bash src/nn/training/script.sh init
!bash src/nn/training/script.sh build_preprocess

# Convert .binpack to .halfkp
%%shell
./build/Release/nn_preprocess --infile $DATA_DIR/gensfen.binpack --outfile $LOCAL_DATA_DIR/gensfen.halfkp

# Check if files are ready
%%shell
ls src/nn/training/main.py
ls $LOCAL_DATA_DIR/gensfen.halfkp

# Training
%shell
python src/nn/training/main.py \
  --dataset=$LOCAL_DATA_DIR/gensfen.halfkp \
  --test-dataset=$LOCAL_DATA_DIR/gensfen-test.halfkp \
  --checkpoint-dir=$DATA_DIR \
  --batch-size=$((1 << 13)) \
  --loss-mode=mse \
  --command=train
```

Gensfen on Google Colab

```
# Build stockfish
!bash src/nn/training/script.sh init
!bash src/nn/training/script.sh build_stockfish

# Generate "gensfen.bin"
!SIZE=100000000 OPTIONS="save_every 10000000" bash src/nn/training/script.sh gensfen
```

Jupyter notebook for visualization

```
jupyter notebook src/nn/training/main.ipynb
```

Visualize HalfKP layer using Embedding Projector

- URL: http://projector.tensorflow.org/?config=https://hi-ogawa.github.io/hosting/toy-chess/projector-config.json
- Note that the data is hosted at https://github.com/hi-ogawa/hosting/tree/master/toy-chess.
  Here we only generate the data.

```
# "regex" filters which row to export by matching to "<king square>_<piece type>_<piece square>"

# For example, ".1" will match only king at backrank
python src/nn/training/main.py \
  --checkpoint=src/nn/data/ckpt.pt \
  --data-tsv=src/nn/data/embedding-data--backrank.tsv \
  --meta-tsv=src/nn/data/embedding-meta--backrank.tsv \
  --regex='.1' \
  --command=export_embedding

# Take only king at initial position "e1"
python src/nn/training/main.py \
  --checkpoint=src/nn/data/ckpt.pt \
  --data-tsv=src/nn/data/embedding-data--e1.tsv \
  --meta-tsv=src/nn/data/embedding-meta--e1.tsv \
  --regex='e1' \
  --command=export_embedding
```

![HalfKP Embedding](https://hi-ogawa.github.io/hosting/toy-chess/halfkp-embedding.gif)

===

Move prediction

- Dataset: http://ccrl.chessdom.com/ccrl/4040/games.html
- Colab notebook: https://colab.research.google.com/drive/12SRSmt-n1eXMuinl6MNc0a1u-xs0UFG6

```
# Convert from .pgn to .slim-pgn
dos2unix < src/nn/data/CCRL-4040.[1206633].pgn | build/Release/slim_pgn > src/nn/data/CCRL-4040.[1206633].slim-pgn

# Shuffle lines (because CCRL's pgn are chronologically ordered from 2005 to 2020)
shuf src/nn/data/CCRL-4040.[1206633].slim-pgn > src/nn/data/CCRL-4040.[1206633].shuf.slim-pgn

# Convert from .slim-pgn to .halfkp-move (this further shuffles position-move pairs)
build/Release/nn_preprocess_move --infile src/nn/data/CCRL-4040.[1206633].shuf.slim-pgn

# Training
python src/nn/training/main_move.py \
  --dataset=src/nn/data/CCRL-4040.[1206633].shuf.halfkp-move \
  --checkpoint-dir=src/nn/data \
  --batch-size=$((1 << 13)) \
  --command=train

# Convert model pramaters to binary for c++ runtime
python src/nn/training/main_move.py \
  --checkpoint=src/nn/data/ckpt-move.pt \
  --weight-file=src/nn/data/ckpt-move.bin \
  --command=process_model_parameters
```


References

- https://github.com/nodchip/Stockfish
- https://github.com/glinscott/nnue-pytorch
- https://github.com/david-carteau/cerebrum
