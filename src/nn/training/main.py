import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
from tqdm import tqdm
from datetime import datetime
import os

#
# Model
#

WIDTH1 = 10 * 64 * 64 # input = { (piece-type, piece-position, king-position) }
WIDTH2 = 128
WIDTH3 = 32
WIDTH4 = 32
EMBEDDING_WIDTH = WIDTH1 + 1
EMBEDDING_PAD = WIDTH1

DTYPE = torch.float32

class MyModel(nn.Module):
  def __init__(self):
    super(MyModel, self).__init__()
    self.embedding = nn.Embedding(EMBEDDING_WIDTH, WIDTH2, padding_idx=EMBEDDING_PAD)
    init_scale = np.sqrt(2 * 3 / WIDTH1) # cf. nn.init.kaiming_uniform_
    nn.init.uniform_(self.embedding.weight, -init_scale, init_scale)
    self.l1_bias = torch.nn.Parameter(torch.zeros(WIDTH2)) # NOTE: Not used currently
    self.l2 = nn.Linear(2 * WIDTH2, WIDTH3)
    self.l3 = nn.Linear(WIDTH3, WIDTH4)
    self.l4 = nn.Linear(WIDTH4, 1)

  def load_embedding(self, weight):
    self.embedding = nn.Embedding.from_pretrained(weight, padding_idx=EMBEDDING_PAD)

  def forward(self, x_w, x_b):
    x_w = self.embedding(x_w)
    x_b = self.embedding(x_b)
    x_w = torch.sum(x_w, dim=-2)
    x_b = torch.sum(x_b, dim=-2)
    x = torch.cat([x_w, x_b], dim=-1)
    x = F.relu(x)
    x = self.l2(x)
    x = F.relu(x)
    x = self.l3(x)
    x = F.relu(x)
    x = self.l4(x)
    return x

#
# Metric
#

class MyMetric:
  def __init__(self):
    self.mom = [0, 0]
    self.l1 = 0
    self.cnt = 0
    self.cnt_correct = 0
    self.loss = 0

  def update(self, y1, y2, loss):
    self.cnt += y1.numel()
    self.mom[0] += torch.sum(y1 - y2)
    self.mom[1] += torch.sum((y1 - y2) ** 2)
    self.l1 += torch.sum(torch.abs(y1 - y2))
    self.cnt_correct += torch.sum((y1 >= 0) == (y2 >= 0))
    self.loss += loss

  def compute(self):
    mean = self.mom[0] / self.cnt
    second = self.mom[1] / self.cnt
    var = second - mean ** 2
    std = torch.sqrt(var)
    l1 = self.l1 / self.cnt
    accuracy = self.cnt_correct / self.cnt
    loss = self.loss / self.cnt
    return mean, std, l1, accuracy, loss


#
# Dataset
#

class MyBatchDataset(torch.utils.data.Dataset):
  """Read batch directly from file (assuming data is already shuffled)"""

  def __init__(self, file, batch_size):
    super(MyBatchDataset, self).__init__()
    self.file = file
    self.batch_size = batch_size
    self.size = os.stat(self.file).st_size // 128

  def read_data(self, i):
    count = 64 * self.batch_size # in two bytes
    offset = i * 128 * self.batch_size # in byte
    data = np.fromfile(self.file, dtype=np.uint16, count=count, offset=offset).reshape([-1, 64])
    return data

  def __getitem__(self, i):
    # Read binary as numpy array
    data = self.read_data(i)
    y = data[:, -1].astype(np.int16).reshape([-1, 1])
    x_w = data[:, :32]
    x_b = data[:, 32:]
    x_b[:, -1] = EMBEDDING_PAD # Reset last empty column
    # Convert to torch tensor
    x_w = torch.tensor(x_w.astype(np.long))
    x_b = torch.tensor(x_b.astype(np.long))
    y = torch.tensor(y.astype(np.float32))
    return x_w, x_b, y

  def __len__(self):
    return (self.size + self.batch_size - 1) // self.batch_size

#
# Training
#

DEVICE = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

MODEL_INIT_SEED = 0x12345678

# NOTE: These normalization only affects learning rate scaling and not network's inherent expressiveness
# Cf. PawnValueEg and Tempo from types.h
SCORE_SCALE = 208
SCORE_TEMPO = 28

def normalize_score(y):
  return (y - SCORE_TEMPO) / SCORE_SCALE


def my_loss_func(y_model, y_target, loss_mode):
  assert loss_mode in ["mse", "bce"]

  # [ Squared evaluation error ]
  if loss_mode == "mse":
    return F.mse_loss(y_model, y_target)

  # [ Win/Loss probability error ]
  # NOTE: this mode have less effect from too high/low evaluation
  if loss_mode == "bce":
    return F.binary_cross_entropy_with_logits(y_model, torch.sigmoid(y_target))


def train(dataset_file, test_dataset_file, checkpoint_embedding, ckpt_file, ckpt_dir, num_epochs, batch_size, num_workers, weight_decay, learning_rate, loss_mode, scheduler_patience, **kwargs):
  print(":: Loading dataset")
  train_dataset = MyBatchDataset(dataset_file, batch_size)
  if test_dataset_file:
    test_dataset = MyBatchDataset(test_dataset_file, batch_size)

  train_loader = torch.utils.data.DataLoader(train_dataset, batch_size=None, num_workers=num_workers, pin_memory=True)
  if test_dataset_file:
    test_loader = torch.utils.data.DataLoader(test_dataset, batch_size=None, num_workers=num_workers, pin_memory=True)

  print(":: Loading model")
  torch.manual_seed(MODEL_INIT_SEED)
  model = MyModel()
  model.to(DEVICE)
  if checkpoint_embedding:
    embedding_weight = torch.load(checkpoint_embedding, map_location=DEVICE)['model_state_dict']['embedding.weight']
    model.load_embedding(embedding_weight)
  print(model)

  optimizer = torch.optim.Adam(model.parameters(), lr=learning_rate, weight_decay=weight_decay)
  scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, factor=0.5, patience=scheduler_patience)
  init_epoch = 0

  if ckpt_file is not None:
    print(f":: Loading checkpoint ({ckpt_file})")
    ckpt = torch.load(ckpt_file, map_location=DEVICE)
    model.load_state_dict(ckpt["model_state_dict"])
    if checkpoint_embedding:
      embedding_weight = torch.load(checkpoint_embedding, map_location=DEVICE)['model_state_dict']['embedding.weight']
      model.load_embedding(embedding_weight)
    optimizer.load_state_dict(ckpt["optimizer_state_dict"])
    scheduler.load_state_dict(ckpt["scheduler_state_dict"])
    init_epoch = ckpt["epoch"] + 1

  print(f":: Start training")
  for epoch in range(init_epoch, num_epochs):
    lr = optimizer.state_dict()['param_groups'][0]['lr']
    print(f":: Epoch = {epoch}, LR = {lr}")
    print(f":: Training loop")
    metric = MyMetric()
    model.train()
    with tqdm(train_loader) as progressbar:
      for i, batch_sample in enumerate(progressbar):
        optimizer.zero_grad()
        x_w, x_b, y_target = [t.to(DEVICE) for t in batch_sample]
        y_target = normalize_score(y_target)
        y_model = model(x_w, x_b)
        loss = my_loss_func(y_model, y_target, loss_mode)
        loss.backward()
        optimizer.step()
        metric.update(y_model.detach().cpu(), y_target.detach().cpu(), loss.detach().cpu() * y_model.numel())
        _mean, _std, run_l1, run_acc, run_loss = metric.compute()
        progressbar.set_postfix(loss=float(loss), run_loss=float(run_loss), run_acc=float(run_acc), run_l1=float(run_l1))
        del x_w, x_b, y_target, y_model
    _mean, _std, _l1, train_acc, train_loss = metric.compute()

    test_loss = 0
    test_acc = 0
    if test_dataset_file:
      print(f":: Validation loop")
      metric = MyMetric()
      model.eval()
      for batch_sample in tqdm(test_loader):
        x_w, x_b, y_target = [t.to(DEVICE) for t in batch_sample]
        y_target = normalize_score(y_target)
        y_model = model(x_w, x_b)
        loss = my_loss_func(y_model, y_target, loss_mode)
        metric.update(y_model.detach().cpu(), y_target.detach().cpu(), loss.detach().cpu() * y_model.numel())
        del x_w, x_b, y_target, y_model
        _mean, _std, run_l1, run_acc, run_loss = metric.compute()
        progressbar.set_postfix(loss=float(loss), run_loss=float(run_loss), run_acc=float(run_acc), run_ls=float(run_l1))
      _mean, _std, _l1, _acc, test_loss = metric.compute()
      scheduler.step(test_loss)

    if ckpt_dir is not None:
      ckpt = {
        "model_state_dict": model.state_dict(),
        "optimizer_state_dict": optimizer.state_dict(),
        "scheduler_state_dict": scheduler.state_dict(),
        "epoch": epoch,
      }
      timestamp = datetime.strftime(datetime.now(), "%F-%H-%M-%S")
      ckpt_file = f"{ckpt_dir}/ckpt-{timestamp}-epoch-{epoch}-acc-{float(train_acc):.3}-{float(test_acc):.3}-loss-{float(train_loss):.3}-{float(test_loss):.3}-lr-{float(lr)}.pt"
      print(f":: Saving checkpoint ({ckpt_file})")
      torch.save(ckpt, ckpt_file)

#
# Data analysis
#

# NOTE: Plotting only works in jupyter notebook
def plot_statistics(infile, batch_size, **kwargs):
  import pandas as pd
  import matplotlib.pyplot as plt
  print(":: Loading dataset")
  dataset = MyBatchDataset(infile, batch_size=batch_size)
  total = len(dataset)
  ys = []
  for i in tqdm(range(total), total=len(dataset)):
    data = dataset.read_data(i)
    ys.append(data[:, -1].copy())
  y = pd.Series(np.concatenate(ys).astype(np.int16))
  print(":: Computing statistics")
  print(y.describe())
  print(":: Plotting histogram")
  y.plot.hist(bins=np.linspace(-4000, 4000, 100))


def rename_model_parameters(state_dict):
  import matplotlib.pyplot as plt
   # NOTE:
   #  Embedding weight shape is (in-dim, out-dim)
   #  but, linear weight shape is (out-dim, in-dim)
  return {
    'l1.weight': state_dict['embedding.weight'][:WIDTH1],
    'l1.bias':   state_dict['l1_bias'],
    'l2.weight': state_dict['l2.weight'],
    'l2.bias':   state_dict['l2.bias'],
    'l3.weight': state_dict['l3.weight'],
    'l3.bias':   state_dict['l3.bias'],
    'l4.weight': state_dict['l4.weight'],
    'l4.bias':   state_dict['l4.bias']
  }


def plot_model_parameters(infile):
  import pandas as pd
  import matplotlib.pyplot as plt

  print(f":: Loading checkpoint ({infile})")
  parameters = torch.load(infile, map_location=DEVICE)["model_state_dict"]
  parameters = rename_model_parameters(parameters)

  print(f":: Plotting histograms")
  for name, tensor in parameters.items():
    print(f":: [{name}]")
    x = np.array(tensor.reshape(-1))
    print(pd.Series(x).describe())
    plt.hist(x, bins=1000)
    plt.show()


def process_model_parameters(infile, outfile):
  print(f":: Loading checkpoint ({infile})")
  parameters = torch.load(infile, map_location=DEVICE)["model_state_dict"]
  parameters = rename_model_parameters(parameters)
  print(f":: Writing weight ({outfile})")
  with open(outfile, 'wb') as f:
    for tensor in parameters.values():
      f.write(bytes(np.array(tensor)))

  # Veryfy size
  expected = 0
  expected += (WIDTH1 + 1) * WIDTH2
  expected += (2 * WIDTH2 + 1) * WIDTH3
  expected += (WIDTH3 + 1) * WIDTH4
  expected += (WIDTH4 + 1)
  expected *= 4
  assert os.stat(outfile).st_size == expected


# Export data for embedding projection visualization
def export_embedding(infile, data_tsv, meta_tsv, regex, **kwargs):
  assert data_tsv and meta_tsv

  print(f":: Loading checkpoint ({infile})")
  parameters = torch.load(infile, map_location=DEVICE)["model_state_dict"]
  data = parameters['embedding.weight'][:WIDTH1].numpy() # shape = (WIDTH1, WIDTH2)

  # Generate labels <king square>_<piece type>_<piece square> (e.g. e1_wQ_d1)
  piece_types = ['wP', 'wN', 'wB', 'wR', 'wQ', 'bP', 'bN', 'bB', 'bR', 'bQ']
  squares = [(chr(ord('a') + j) + str(i + 1)) for j in range(8) for i in range(8)]
  labels = []
  for k_sq in squares:
      for sq in squares:
          for pt in piece_types:
              labels.append(f"{k_sq}_{pt}_{sq}")

  print(f":: Writing data ({data_tsv})")
  import re
  f1 = open(data_tsv, 'w')
  f2 = open(meta_tsv, 'w')
  for label, row in zip(labels, data):
    if not re.match(regex, label): continue
    print(*row, sep="\t", file=f1)
    print(label, file=f2)


#
# Main
#

COMMANDS = [
  "train",
  "plot_statistics",
  "plot_model_parameters",
  "process_model_parameters",
  "export_embedding"
]

def main(command, dataset, test_dataset, checkpoint, checkpoint_dir, weight_file, **kwargs):
  if command == "process_model_parameters":
    assert checkpoint and weight_file
    process_model_parameters(infile=checkpoint, outfile=weight_file)

  if command == "plot_model_parameters":
    assert checkpoint
    plot_model_parameters(infile=checkpoint)

  if command == "plot_statistics":
    assert dataset
    plot_statistics(infile=dataset, **kwargs)

  if command == "export_embedding":
    export_embedding(infile=checkpoint, **kwargs)

  if command == "train":
    train(
      dataset_file=dataset,
      test_dataset_file=test_dataset,
      ckpt_file=checkpoint,
      ckpt_dir=checkpoint_dir,
      **kwargs)


def main_cli():
  import sys, argparse
  parser = argparse.ArgumentParser()
  parser.add_argument("--dataset", type=str)
  parser.add_argument("--test-dataset", type=str)
  parser.add_argument("--checkpoint-embedding", type=str)
  parser.add_argument("--checkpoint", type=str)
  parser.add_argument("--checkpoint-dir", type=str)
  parser.add_argument("--weight-file", type=str)
  parser.add_argument("--num-epochs", type=int, default=1024)
  parser.add_argument("--batch-size", type=int, default=1024)
  parser.add_argument("--num-workers", type=int, default=0)
  parser.add_argument("--weight-decay", type=float, default=0)
  parser.add_argument("--learning-rate", type=float, default=0.001)
  parser.add_argument("--scheduler-patience", type=float, default=0)
  parser.add_argument("--loss-mode", type=str, default="mse")
  parser.add_argument("--data-tsv", type=str)
  parser.add_argument("--meta-tsv", type=str)
  parser.add_argument("--regex", type=str, default='.')
  parser.add_argument("--command", type=str, choices=COMMANDS, default="train")
  sys.exit(main(**parser.parse_args().__dict__))


if __name__ == "__main__":
  main_cli()
