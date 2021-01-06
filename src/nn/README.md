Setup

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

Training on Google Colab (example: https://colab.research.google.com/drive/1r01G0vpKv9HBA06uOMk9RoRydihCpoUi)

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
!git clone --single-branch --branch nn-training https://github.com/hi-ogawa/toy-chess.git
%cd /content/toy-chess

# Build "nn_preprocess"
!bash src/nn/training/script.sh init
!bash src/nn/training/script.sh build_preprocess

# Convert .binpack to .halfkp
%%shell
./build/Release/nn_preprocess --infile $DATA_DIR/gensfen.binpack --outfile $LOCAL_DATA_DIR/gensfen.halfkp

# Check files are ready
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

References

- https://github.com/nodchip/Stockfish
- https://github.com/glinscott/nnue-pytorch
- https://github.com/david-carteau/cerebrum
