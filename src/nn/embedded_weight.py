

def main(infile):
  with open(infile, 'rb') as f:
    data = f.read()
  size = len(data)
  data_literal = ('+' + data.hex('+')).replace('+', '\\x')
  print("namespace nn {")
  print("  extern const char* kEmbeddedWeight;")
  print("  extern const size_t kEmbeddedWeightSize;")
  print("  const char* kEmbeddedWeight = \"", data_literal, "\";", sep="")
  print("  const size_t kEmbeddedWeightSize = ", size, ";", sep="")
  print("}")


if __name__ == '__main__':
  import sys
  assert sys.argv[1]
  main(sys.argv[1])
