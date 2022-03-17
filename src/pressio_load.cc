#include <libpressio_ext/cpp/json.h>
#include <libpressio_ext/cpp/pressio.h>
#include <libpressio_ext/cpp/printers.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "roibin_test_version.h"

const std::string usage = R"(pressio_load
code to test loading pressio_compressor from json

-h print this message
-v print the version information
)";

struct cmdline_args {
  std::string infile;
};

using namespace std::string_literals;

cmdline_args parse_args(int argc, char* argv[]) {
  cmdline_args args;

  int opt;
  while ((opt = getopt(argc, argv, "hvf:")) != -1) {
    switch (opt) {
      case 'f':
        args.infile = optarg;
        break;
      case 'h':
        std::cout << usage << std::endl;
        ;
        exit(0);
        break;
      case 'v':
        std::cout << ROIBIN_TEST_VERSION << std::endl;
        exit(0);
        break;
    }
  }

  return args;
}

int main(int argc, char* argv[]) {
  auto args = parse_args(argc, argv);
  std::ifstream ifile(args.infile);
  nlohmann::json j;
  ifile >> j;
  pressio_options options_from_file(j);

  pressio library;
  pressio_compressor comp = library.get_compressor("pressio");
  comp->set_name("pressio");
  comp->set_options(options_from_file);
  comp->set_options({{"pressio:metric", "composite"},
                     {"composite:plugins", std::vector<std::string>{"size", "time"}}});
  std::cout << comp->get_options() << std::endl;

  pressio_data metadata = pressio_data::owning(pressio_float_dtype, {500, 500, 100});
  pressio_data compressed = pressio_data::empty(pressio_byte_dtype, {});
  pressio_data decompressed = pressio_data::clone(metadata);
  auto io = library.get_io("posix");
  io->set_options(
      {{"io:path", "/home/runderwood/git/datasets/hurricane/100x500x500/CLOUDf48.bin.f32"}});
  pressio_data indata = std::move(*io->read(&metadata));

  if (comp->compress(&indata, &compressed) > 0) {
    std::cerr << comp->error_msg() << std::endl;
    exit(comp->error_code());
  }
  if (comp->decompress(&compressed, &decompressed) > 0) {
    std::cerr << comp->error_msg() << std::endl;
    exit(comp->error_code());
  }
  std::cout << comp->get_metrics_results() << std::endl;
}
