#
# Generate embedded_weight.cpp (see CMakeFile.txt for how this script is used)
#

import sys

def main(infile):
  with open(infile, 'rb') as f:
    data = f.read()
  size = len(data)
  # NOTE: bytes.hex(<seperator>) is from python 3.8
  if sys.version_info >= (3, 8):
    data_literal = ('+' + data.hex('+')).replace('+', '\\x')
  else:
    h = data.hex()
    data_literal = ''.join('\\x' + x + y for x, y in zip(h[0::2], h[1::2]))
  print("namespace nn {")
  print("  extern const char* kEmbeddedWeight;")
  print("  extern const int kEmbeddedWeightSize;")
  print("  const char* kEmbeddedWeight = \"", data_literal, "\";", sep="")
  print("  const int kEmbeddedWeightSize = ", size, ";", sep="")
  print("}")


if __name__ == '__main__':
  import sys
  assert sys.argv[1]
  main(sys.argv[1])
