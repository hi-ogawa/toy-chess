#
# Generate embedded_weight.cpp (see CMakeFile.txt for how this script is used)
#

import sys


def read_file(file):
  with open(file, 'rb') as f:
    data = f.read()

  # Convert python bytes into C++ char string literal
  if sys.version_info >= (3, 8):
    data_literal = ('+' + data.hex('+')).replace('+', '\\x') # NOTE: bytes.hex(<seperator>) is from python 3.8
  else:
    h = data.hex()
    data_literal = ''.join('\\x' + x + y for x, y in zip(h[0::2], h[1::2]))

  return data_literal, len(data)


def main(file1, file2):
  data1, size1 = read_file(file1)
  data2, size2 = read_file(file2)
  print("namespace nn {")
  print("  extern const char* kEmbeddedEvalWeight;")
  print("  extern const int   kEmbeddedEvalWeightSize;")
  print("  extern const char* kEmbeddedMoveWeight;")
  print("  extern const int   kEmbeddedMoveWeightSize;")
  print("  const char* kEmbeddedEvalWeight = \""  , data1, "\";", sep="")
  print("  const int   kEmbeddedEvalWeightSize = ", size1, ";",   sep="")
  print("  const char* kEmbeddedMoveWeight = \"",   data2, "\";", sep="")
  print("  const int   kEmbeddedMoveWeightSize = ", size2, ";",   sep="")
  print("}")


if __name__ == '__main__':
  import sys
  assert len(sys.argv) >= 3
  main(sys.argv[1], sys.argv[2])
