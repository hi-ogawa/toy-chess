#
# Predict "from/to"
#

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

WIDTH1 = 10 * 64 * 64
WIDTH2 = 128
EMBEDDING_WIDTH = WIDTH1 + 1
EMBEDDING_PAD = WIDTH1
WIDTH_MOVE1 = 64 * 64
WIDTH_EVAL1 = 32
WIDTH_EVAL2 = 32
WIDTH_EVAL3 = 1

DTYPE = torch.float32

class MyModel(nn.Module):
  def __init__(self):
    super(MyModel, self).__init__()
    # Embedding HalfKP
    self.embedding = nn.Embedding(EMBEDDING_WIDTH, WIDTH2, padding_idx=EMBEDDING_PAD)
    init_scale = np.sqrt(2 * 3 / WIDTH1) # cf. nn.init.kaiming_uniform_
    nn.init.uniform_(self.embedding.weight, -init_scale, init_scale)

    # Move
    self.m_l1 = nn.Linear(2 * WIDTH2, WIDTH_MOVE1)

    # Eval
    self.e_l1 = nn.Linear(2 * WIDTH2, WIDTH_EVAL1)
    self.e_l2 = nn.Linear(WIDTH_EVAL1, WIDTH_EVAL2)
    self.e_l3 = nn.Linear(WIDTH_EVAL2, WIDTH_EVAL3)

  def load_embedding(self, weight):
    self.embedding = nn.Embedding.from_pretrained(weight, padding_idx=EMBEDDING_PAD, freeze=False)

  def forward(self, x_w, x_b):
    x_w = self.embedding(x_w)
    x_b = self.embedding(x_b)
    x_w = torch.sum(x_w, dim=-2)
    x_b = torch.sum(x_b, dim=-2)
    x = torch.cat([x_w, x_b], dim=-1)
    y = z = F.relu(x)
    # Move
    y = self.m_l1(y)
    # Eval
    z = self.e_l1(z)
    z = F.relu(z)
    z = self.e_l2(z)
    z = F.relu(z)
    z = self.e_l3(z)
    return y, z

#
# Metric
#

class MyMetric:
  def __init__(self):
    self.cnt = 0
    self.cnt_correct_y = 0
    self.cnt_correct_z = 0
    self.sum_x = 0

  def update(self, y_model, y_target, z_model, z_target, x):
    y_model = y_model.detach().cpu()
    y_target = y_target.detach().cpu()
    z_model = z_model.detach().cpu()
    z_target = z_target.detach().cpu()
    x = x.detach().cpu()
    self.cnt += y_model.shape[0]
    self.cnt_correct_y += torch.sum(torch.argmax(y_model, dim=-1) == y_target)
    self.cnt_correct_z += torch.sum((z_model >= 0) == (z_target >= 0))
    self.sum_x += x

  def compute(self):
    acc_y = self.cnt_correct_y / self.cnt
    acc_z = self.cnt_correct_z / self.cnt
    avg_x = self.sum_x / self.cnt
    return acc_y, acc_z, avg_x


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
    # Read binary as numpy array (uint16)
    data = self.read_data(i)
    x_w = data[:, :31]
    x_b = data[:, 31:62]
    y = data[:, 62]
    z = data[:, 63].astype(np.int16).astype(np.float32).reshape([-1, 1]) / 100
    # Convert to torch tensor (NOTE: uint16 is not supported by pytorch)
    x_w = torch.tensor(x_w.astype(np.long))
    x_b = torch.tensor(x_b.astype(np.long))
    y = torch.tensor(y.astype(np.long))
    z = torch.tensor(z)
    return x_w, x_b, y, z

  def __len__(self):
    return (self.size + self.batch_size - 1) // self.batch_size

#
# Training
#

DEVICE = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

MODEL_INIT_SEED = 0x12345678

def my_loss_func(y_model, y_target, z_model, z_target):
  return F.cross_entropy(y_model, y_target) + F.mse_loss(z_model, z_target)


def train(dataset_file, test_dataset_file, checkpoint_embedding, ckpt_file, ckpt_dir, num_epochs, batch_size, num_workers, weight_decay, learning_rate, scheduler_patience):
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
    print(f":: --- Training ---")
    metric = MyMetric()
    model.train()
    with tqdm(train_loader) as progressbar:
      for batch_sample in progressbar:
        optimizer.zero_grad()
        x_w, x_b, y_target, z_target = [t.to(DEVICE) for t in batch_sample]
        y_model, z_model = model(x_w, x_b)
        loss = my_loss_func(y_model, y_target, z_model, z_target)
        loss.backward()
        optimizer.step()
        metric.update(y_model, y_target, z_model, z_target, loss * y_model.shape[0])
        acc_y_run, acc_z_run, loss_run = metric.compute()
        progressbar.set_postfix(loss=float(loss), loss_run=float(loss_run), acc_y_run=float(acc_y_run), acc_z_run=float(acc_z_run))
        del x_w, x_b, y_target, y_model
    train_acc, _, train_loss = metric.compute()

    test_acc = 0
    test_loss = 0
    if test_dataset_file is None:
      scheduler.step(train_loss)
    else:
      print(f":: --- Validation ---")
      metric = MyMetric()
      model.eval()
      for batch_sample in tqdm(test_loader):
        x_w, x_b, y_target, z_target = [t.to(DEVICE) for t in batch_sample]
        y_model, z_model = model(x_w, x_b)
        loss = my_loss_func(y_model, y_target, z_model, z_target)
        metric.update(y_model, y_target, z_model, z_target, loss * y_model.shape[0])
        acc_y_run, acc_z_run, loss_run = metric.compute()
        progressbar.set_postfix(loss=float(loss), loss_run=float(loss_run), acc_y_run=float(acc_y_run), acc_z_run=float(acc_z_run))
        del x_w, x_b, y_target, y_model
      test_acc, _, test_loss = metric.compute()
      scheduler.step(test_loss)

    if ckpt_dir is not None:
      ckpt = {
        "model_state_dict": model.state_dict(),
        "optimizer_state_dict": optimizer.state_dict(),
        "scheduler_state_dict": scheduler.state_dict(),
        "epoch": epoch,
      }
      timestamp = datetime.strftime(datetime.now(), "%F-%H-%M-%S")
      ckpt_file = f"{ckpt_dir}/ckpt-move-{timestamp}-epoch-{epoch}-acc-{float(train_acc):.3}-{float(test_acc):.3}-loss-{float(train_loss):.3}-{float(test_loss):.3}-lr-{float(lr)}.pt"
      print(f":: Saving checkpoint ({ckpt_file})")
      torch.save(ckpt, ckpt_file)


#
# Main
#

COMMANDS = [
  "train",
]

def main(command, dataset, test_dataset, checkpoint, checkpoint_dir, weight_file, **kwargs):
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
  parser.add_argument("--command", type=str, choices=COMMANDS, default="train")
  sys.exit(main(**parser.parse_args().__dict__))


if __name__ == "__main__":
  main_cli()