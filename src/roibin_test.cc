#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>
#include <libpressio_ext/cpp/pressio.h>
#include <libpressio_ext/cpp/printers.h>
#include <libpressio_ext/cpp/json.h>
#include <nlohmann/json.hpp>
#include "cleanup.h"
#include "roibin_test_version.h"
#include "hdf5_helpers.h"

const std::string usage = R"(roibin_test
experimental code to test roibin_sz3

-c chunk_size
-f cxi_filename
-h print this message
-v print the version information
)";

struct cmdline_args {
  std::string cxi_filename = "cxic0415_0101.cxi";
  std::string pressio_config_file = "share/blosc.json";
  size_t chunk_size = 1;
};

using namespace std::string_literals;

cmdline_args parse_args(int argc, char* argv[]) {
  cmdline_args args;


  int opt;
  while((opt = getopt(argc, argv, "c:hvf:")) != -1) {
    switch (opt) {
      case 'c':
        args.chunk_size = atoi(optarg);
        if(args.chunk_size == 0) {
          throw std::runtime_error("invalid chunk_size"s + optarg);
        }
        break;
      case 'f':
        args.cxi_filename = optarg;
        break;
      case 'p':
        args.pressio_config_file = optarg;
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

int main(int argc, char *argv[])
{
  int world_rank, world_size, per_node_rank;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  //create communicators of ranks within each node
  MPI_Comm per_node_comm;
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, world_rank, MPI_INFO_NULL, &per_node_comm);
  MPI_Comm_rank(per_node_comm, &per_node_rank);
  cleanup cleanup_per_node_rank([&]{MPI_Comm_free(&per_node_comm);});

  //create communicators with 1 rank per node
  int work_rank, work_size;
  MPI_Comm work_comm;
  MPI_Comm_split(MPI_COMM_WORLD, per_node_rank, world_rank, &work_comm);
  cleanup cleanup_work_comm([&]{MPI_Comm_free(&work_comm);});
  MPI_Comm_rank(work_comm, &work_rank);
  MPI_Comm_size(work_comm, &work_size);


  if(per_node_rank == 0) {
    try{
      auto args = parse_args(argc, argv);

      hid_t fapl = check_hdf5(H5Pcreate(H5P_FILE_ACCESS)); 
      cleanup cleanup_fapl([&]{ H5Pclose(fapl); });
      check_hdf5(H5Pset_fapl_mpio(fapl, work_comm, MPI_INFO_NULL));

      hid_t cxi = check_hdf5(H5Fopen(args.cxi_filename.c_str(), H5F_ACC_RDONLY, fapl));
      cleanup cleanup_cxi([&]{H5Fclose(cxi);});

      const char* data_loc="/entry_1/data_1/data";
      const char* peakx_loc="/entry_1/result_1/peakXPosRaw";
      const char* peaky_loc="/entry_1/result_1/peakYPosRaw";
      const char* npeak_loc="/entry_1/result_1/nPeaks";

      auto data = open_dset(cxi, data_loc);
      auto posx = open_dset(cxi, peakx_loc);
      auto posy = open_dset(cxi, peaky_loc);
      auto npeaks = open_dset(cxi, npeak_loc);

      //hdf5 and libpressio use opposite data ordering
      size_t num_events = data.get_dims_hsize().front();
      size_t max_peaks = posx.get_dims_hsize().front();
      size_t total_size = 0;
      size_t total_compressed_size = 0;
      pressio_data peaks_data = pressio_data::owning(
          pressio_int64_dtype, {args.chunk_size}
          );
      pressio_data posx_data = pressio_data::owning(
          pressio_double_dtype, {max_peaks, args.chunk_size}
          );
      pressio_data posy_data = pressio_data::owning(
          pressio_double_dtype, {max_peaks, args.chunk_size}
          );

      auto data_lp_size = data.get_pressio_dims();
      auto data_lp_worksize = data_lp_size;
      data_lp_size.back() = args.chunk_size;
      pressio_data data_data = pressio_data::owning(
          pressio_float_dtype, data_lp_worksize
          );

      //prepare compressor
      std::ifstream ifile(args.pressio_config_file);
      nlohmann::json j;
      ifile >> j;
      pressio_options options_from_file(j);
      pressio library;
      pressio_compressor comp = library.get_compressor("pressio");
      comp->set_name("pressio");
      comp->set_options(options_from_file);
      comp->set_options({
          {"pressio:metric", "composite"},
          {"composite:plugins", std::vector<std::string>{"size", "time"}}
      });

      if(work_rank == 0) {
        std::cout << comp->get_options() << std::endl;
      }

      for (size_t i = 0; i < num_events; i += (args.chunk_size*work_size)) {
          size_t id = i + work_rank * args.chunk_size;
          size_t work_items;
          if(id > num_events) {
            work_items = 0;
          } else if(id+args.chunk_size > num_events) {
            work_items = num_events - id;
          } else {
            work_items = args.chunk_size;
          }

          //read npeaks
          std::vector<hsize_t> npeaks_start{id};
          std::vector<hsize_t> npeaks_count{work_items};
          read(npeaks, npeaks_start, npeaks_count, peaks_data);
          //read posx
          std::vector<hsize_t> posx_start{id, 0};
          std::vector<hsize_t> posx_count{work_items, max_peaks};
          read(posx, npeaks_start, npeaks_count, posx_data);
          //read posy
          std::vector<hsize_t> posy_start{id, 0};
          std::vector<hsize_t> posy_count{work_items, max_peaks};
          read(posy, npeaks_start, npeaks_count, posy_data);

          //compute centers
          size_t peaks_in_work = 0;
          auto npeaks_ptr = static_cast<const int64_t*>(peaks_data.data());
          std::vector<size_t> peaks_to_events, to_start_of_event;
          peaks_to_events.reserve(max_peaks * work_items);
          to_start_of_event.reserve(work_items* work_items);
          for (size_t i = 0; i < work_items; ++i) {
            peaks_in_work += npeaks_ptr[i];
            for (int64_t j = 0; j < npeaks_ptr[i]; ++j) {
              peaks_to_events.push_back(i);
              to_start_of_event.push_back(j);
            }
          }
          pressio_data centers = pressio_data::owning(pressio_uint64_dtype,
              {3, peaks_in_work});
          auto posx_ptr = static_cast<double const*>(posx_data.data());
          auto posy_ptr = static_cast<double const*>(posy_data.data());
          auto centers_ptr = static_cast<uint64_t*>(posy_data.data());
          for (size_t i = 0; i < peaks_in_work; ++i) {
            centers_ptr[i*3] = static_cast<size_t>(posx_ptr[peaks_to_events[i] * max_peaks + to_start_of_event[i]]);
            centers_ptr[i*3+1] =  static_cast<size_t>(posy_ptr[peaks_to_events[i] * max_peaks + to_start_of_event[i]]);
            centers_ptr[i*3+2] =  peaks_to_events[i];
          }

          //read data
          std::vector<hsize_t> data_start{id, 0};
          std::vector<hsize_t> data_count{work_items, max_peaks};
          read(data, data_start, data_count, data_data);

          //trigger compression/decompression
          comp->set_options({
              {"roibin:centers", std::move(centers)}
          });
          pressio_data data_comp = pressio_data::empty(pressio_byte_dtype, {});
          comp->compress(&data_data, &data_comp);
          
          //save metrics worth saving
          total_compressed_size += 1;
          total_size += 1;
      }
      //compute global compression ratio
      if(work_rank == 0) {
        std::cout << "global_cr=" << total_size/static_cast<double>(total_compressed_size)  << std::endl;
      }


    } catch(std::exception const& ex) {
      std::cout << ex.what() << std::endl;
    }
  }

  MPI_Finalize();
  return 0;
}
