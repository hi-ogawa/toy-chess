MCTS self play training

- training data generation by self play data
  - MCTS episodes
  - (position, policy target, win-draw-loss target)

  - input: weight file
  - output: dataset

- training
  - input: weight file + dataset
  - output: weight file

- pitting new/old models
  - input: old weight file + new weight file
  - output: replace or not


```bash
python src/zero/main.py initialize_checkpoint --checkpoint-file src/zero/data/checkpoint-00.pt

python src/zero/main.py export_weight_file --checkpoint-file src/zero/data/checkpoint-00.pt --weight-file src/zero/data/weight-00.bin

./build/Release/zero_selfplay --weight-file src/zero/data/weight-00.bin --output-file src/zero/data/selfplay-00.bin --exclude-draw 1

cp src/zero/data/checkpoint-00.pt src/zero/data/checkpoint-00-train.pt

python src/zero/main.py train --checkpoint-file=src/zero/data/checkpoint-00-train.pt --dataset-file src/zero/data/selfplay-00.bin

python src/zero/main.py export_weight_file --checkpoint-file src/zero/data/checkpoint-00-train.pt --weight-file src/zero/data/weight-00-train.bin

./build/Release/zero_pit --first src/zero/data/weight-00.bin --second src/zero/data/weight-00-train.bin
```
