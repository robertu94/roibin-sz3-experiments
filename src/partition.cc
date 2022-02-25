#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <unistd.h>
#include <mpi.h>
#include "roibin_test_version.h"

const std::string usage = R"(pressio_load
code to test loading pressio_compressor from json

-h print this message
-v print the version information
)";

struct cmdline_args {
  size_t tasks = 1;
  size_t chunk_size = 1;
};

using namespace std::string_literals;
using namespace std::chrono_literals;

cmdline_args parse_args(int argc, char* argv[]) {
  cmdline_args args;

  int opt;
  while((opt = getopt(argc, argv, "c:hvn:")) != -1) {
    switch (opt) {
      case 'c':
        {
          args.chunk_size = std::atoi(optarg);
          if(args.chunk_size <= 0) {
            std::cerr << "invalid number of chunks: " << optarg << std::endl;
            exit(1);
          }
        }
        break;
      case 'n':
        {
          args.tasks = std::atoi(optarg);
          if(args.tasks <= 0) {
            std::cerr << "invalid number of tasks: " << optarg << std::endl;
            exit(1);
          }
        }
        break;
      case 'h':
        std::cout << usage << std::endl;;
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
int main(int argc, char* argv[])
{
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  const auto args = parse_args(argc, argv);
  if(rank == 0) {
  std::cout << "size=" << size << " tasks=" << args.tasks << " chunk_size=" << args.chunk_size << std::endl;
  }
  MPI_Barrier(MPI_COMM_WORLD);

  uint64_t total_work_items_per_rank = 0;
  for (size_t i = 0; i < args.tasks; i += (args.chunk_size * size)) {
    size_t id = i + rank * args.chunk_size;
    size_t work_items;
    if(id > args.tasks) {
      work_items = 0;
    } else if(id+args.chunk_size > args.tasks) {
      work_items = args.tasks - id;
    } else {
      work_items = args.chunk_size;
    }
    total_work_items_per_rank += work_items;
    std::cout << i << "=i rank=" << rank << " id=" << id << "work_items=" << work_items << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    std::this_thread::sleep_for(10ms);
  }
  size_t total_work_items = 0;
  MPI_Reduce(&total_work_items_per_rank, &total_work_items,1, MPI_UINT64_T, MPI_SUM,  0, MPI_COMM_WORLD);

  if(rank == 0) {
    std::cout << "total_work_items=" << total_work_items << std::endl;
  }

  MPI_Finalize();
}
