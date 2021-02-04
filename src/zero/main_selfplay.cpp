#include "selfplay.hpp"

int main(int argc, const char* argv[]) {
  Cli cli{argc, argv};
  auto weight_file = cli.getArg<string>("--weight-file");
  auto output_file = cli.getArg<string>("--output-file");
  if (!weight_file || !output_file) {
    std::cerr << cli.help() << std::endl;
    return 1;
  }

  zero::Selfplay selfplay;
  selfplay.weight_file = *weight_file;
  selfplay.output_file = *output_file;
  selfplay.log_level = cli.getArg<int>("--log-level").value_or(1);
  selfplay.num_episodes = cli.getArg<int>("--num-episodes").value_or(1000);
  selfplay.num_simulations = cli.getArg<int>("--num-simulations").value_or(1000);
  selfplay.exclude_draw = cli.getArg<int>("--exclude-draw").value_or(1);
  selfplay.run();
  return 0;
}
