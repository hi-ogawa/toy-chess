import subprocess


def shell(args, **kwargs):
  print("::", args)
  return subprocess.run(args, shell=True, check=True, **kwargs)


def command_init(build_type, directory, clang, rest_args="", **kwargs):
  args = f"cmake -B {directory} -DCMAKE_BUILD_TYPE={build_type}"
  if clang:
    args = "CC=clang CXX=clang++ LDFLAGS=-fuse-ld=lld " + args
  shell(args + " " + rest_args)


def command_build(directory, target, parallel, rest_args="", **kwargs):
  args = f"cmake --build {directory} -j {parallel}"
  if target:
    args += f" -t {target}"
  shell(args + " " + rest_args)


def command_run(directory, no_build, executable, rest_args="", **kwargs):
  if not no_build:
    command_build(directory, **kwargs)
  args = f"{directory}/{executable}" + " " + rest_args
  shell(args)


def main_cli():
  import sys, os, argparse

  rest_args = ""
  if sys.argv.count("--"):
    i = sys.argv.index("--")
    rest_args = " ".join(sys.argv[i + 1:])
    sys.argv[i:] = []

  parser = argparse.ArgumentParser()
  parser.add_argument("command", type=str, choices=["init", "build", "run"])
  parser.add_argument("--clang", action="store_true", default=False)
  parser.add_argument("--directory", type=str)
  parser.add_argument("--build-type", type=str, default="Release")
  parser.add_argument("--parallel", type=int, default=(os.cpu_count() or 1))
  parser.add_argument("--target", type=str)
  parser.add_argument("--no-build", action="store_true", default=False)
  parser.add_argument("--executable", type=str, default="main")

  args = parser.parse_args()
  args.rest_args = rest_args
  if args.directory is None:
    args.directory = f"build/{args.build_type}"

  func = globals()[f"command_{args.command}"]
  try:
    sys.exit(func(**args.__dict__))
  except KeyboardInterrupt:
    print("-- KeyboardInterrupt --")
  except subprocess.CalledProcessError as e:
    print(f"-- {e} --")


if __name__ == "__main__":
  main_cli()
