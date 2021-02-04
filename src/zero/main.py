import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
import os
from tqdm import tqdm

#
# Model
#

WIDTH1 = 10 * 64 * 64 # = 40960
EMBEDDING_WIDTH = WIDTH1 + 1
EMBEDDING_PAD = WIDTH1

WIDTH2 = 256

WIDTH_POLICY = 1792
POLICY_TOP_K = 16

class Model(nn.Module):
  def __init__(self):
    super(Model, self).__init__()
    self.embedding = nn.Embedding(EMBEDDING_WIDTH, WIDTH2, padding_idx=EMBEDDING_PAD)
    self.fc_policy = nn.Linear(2 * WIDTH2, WIDTH_POLICY)
    self.fc_value = nn.Linear(2 * WIDTH2, 1)

  def forward(self, x):
    x_w, x_b = x[:, :32], x[:, 32:]
    x_w = self.embedding(x_w).sum(dim=-2)
    x_b = self.embedding(x_b).sum(dim=-2)
    x = F.relu(torch.cat([x_w, x_b], dim=-1))
    y = self.fc_policy(x)
    z = self.fc_value(x)
    return y, z

  @staticmethod
  def get_loss(y, z, y_index_tgt, y_value_tgt, z_tgt):
    # KL divergence for top-k probabilities
    y = F.log_softmax(y, dim=-1)
    y = y.gather(index=y_index_tgt, dim=-1)
    loss_y = torch.mean(torch.sum(- y_value_tgt * y, dim=1))
    # Sigmoid loss
    loss_z = F.binary_cross_entropy_with_logits(z, z_tgt)
    return loss_y, loss_z, loss_y + loss_z


#
# Dataset
#


def reinterpret_cast(data, dtype):
  n, _m = data.shape
  return data.reshape([-1]).view(dtype).reshape([n, -1])


class Dataset(torch.utils.data.Dataset):
  def __init__(self, file):
    super(Dataset, self).__init__()
    # Byte offsets of "struct Entry" member
    o1 = 2 * 64
    o2 = o1 + 2 * POLICY_TOP_K
    o3 = o2 + 4 * POLICY_TOP_K
    o4 = o3 + 1
    size_of_entry = o4
    data = np.fromfile(file, dtype=np.uint8).reshape([-1, size_of_entry])
    self.data = data
    self.size = len(data)
    self.x =       torch.tensor(reinterpret_cast(data[:, :o1],   np.uint16).astype(np.long))
    self.y_index = torch.tensor(reinterpret_cast(data[:, o1:o2], np.uint16).astype(np.long))
    self.y_value = torch.tensor(reinterpret_cast(data[:, o2:o3], np.float32))
    self.z =       torch.tensor(reinterpret_cast(data[:, o3:o4], np.int8).astype(np.float32))

  def __len__(self):
    return self.size

  def __getitem__(self, i):
    return self.x[i], self.y_index[i], self.y_value[i], self.z[i]


#
# Training
#

MODEL_INIT_SEED = 0x12345678
DEVICE = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

def train(checkpoint_file, dataset_file, num_epochs, learning_rate, batch_size, num_workers, **kwargs):
  print(f":: Loading dataset {dataset_file}")
  dataset = Dataset(dataset_file)
  loader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, num_workers=num_workers, shuffle=True, pin_memory=True)

  print(f":: Loading checkpoint {checkpoint_file}")
  model = Model()
  model.to(DEVICE)
  model.load_state_dict(torch.load(checkpoint_file, map_location=DEVICE)['model_state_dict'])
  print(model)

  optimizer = torch.optim.Adam(model.parameters(), lr=learning_rate)

  print(f":: Start training")
  for epoch in range(num_epochs):
    print(f":: --- Epoch {epoch} ---")
    with tqdm(loader) as tqdm_iter:
      for batch_sample in tqdm_iter:
        optimizer.zero_grad()
        x, y_index_tgt, y_value_tgt, z_tgt = [t.to(DEVICE) for t in batch_sample]
        y, z = model(x)
        loss_y, loss_z, loss = Model.get_loss(y, z, y_index_tgt, y_value_tgt, z_tgt)
        loss.backward()
        tqdm_iter.set_postfix(loss_y=float(loss_y), loss_z=float(loss_z))

  print(f":: Saving checkpoint {checkpoint_file}")
  torch.save({"model_state_dict": model.state_dict()}, checkpoint_file)


#
# Misc
#

def initialize_checkpoint(checkpoint_file, **kwargs):
  assert checkpoint_file
  torch.manual_seed(MODEL_INIT_SEED)
  model = Model()
  print(f":: Saving checkpoint ({checkpoint_file})")
  torch.save({"model_state_dict": model.state_dict()}, checkpoint_file)


def export_weight_file(checkpoint_file, weight_file, **kwargs):
  assert checkpoint_file and weight_file
  print(f":: Loading checkpoint ({checkpoint_file})")
  state_dict = torch.load(checkpoint_file, map_location=DEVICE)["model_state_dict"]

  print(f":: Writing weight ({weight_file})")
  tensors = [
    state_dict['embedding.weight'][:WIDTH1],
    torch.zeros(WIDTH2), # TODO: Not used but c++ runtime still expects this...
    state_dict['fc_policy.weight'],
    state_dict['fc_policy.bias'],
    state_dict['fc_value.weight'],
    state_dict['fc_value.bias'],
  ]
  with open(weight_file, 'wb') as f:
    for t in tensors:
      f.write(bytes(np.array(t)))

  # Veryfy size
  expected = 0
  expected += (WIDTH1 + 1) * WIDTH2
  expected += (2 * WIDTH2 + 1) * WIDTH_POLICY
  expected += (2 * WIDTH2 + 1) * 1
  expected *= 4
  assert os.stat(weight_file).st_size == expected


#
# Main
#

COMMANDS = [
  "train",
  "initialize_checkpoint",
  "export_weight_file",
]


def main_cli():
  import sys, argparse
  parser = argparse.ArgumentParser()
  parser.add_argument("command", type=str, choices=COMMANDS)
  parser.add_argument("--dataset-file", type=str)
  parser.add_argument("--checkpoint-file", type=str)
  parser.add_argument("--checkpoint-dir", type=str)
  parser.add_argument("--weight-file", type=str)
  parser.add_argument("--batch-size", type=int, default=128)
  parser.add_argument("--num-epochs", type=int, default=20)
  parser.add_argument("--learning-rate", type=float, default=0.001)
  parser.add_argument("--num-workers", type=int, default=1)
  parser.add_argument("--command", type=str, choices=COMMANDS, default="train")
  args = parser.parse_args()
  func = globals()[args.command]
  sys.exit(func(**args.__dict__))


if __name__ == "__main__":
  main_cli()
