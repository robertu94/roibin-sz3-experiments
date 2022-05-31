#include <hdf5.h>
#include <libpressio_ext/cpp/json.h>
#include <libpressio_ext/cpp/pressio.h>
#include <libpressio_ext/cpp/printers.h>
#include <mpi.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "cleanup.h"
#include "file_helpers.h"
#include "hdf5_helpers.h"
#include "roibin_test_version.h"

std::string basename(std::string const& base) {
  auto last_slash = base.rfind('/');
  if (base.empty()) {
    return base;
  } else if (base == "/") {
    return base;
  } else if (last_slash == std::string::npos) {
    return base;
  } else if (last_slash + 1 == base.size()) {
    return basename(base.substr(0, base.size() - 1));
  } else {
    return base.substr(last_slash + 1);
  }
}

// clang-format off
const std::string usage = R"(roibin_test experimental code to test roibin_sz3

-c <chunk_size> chunk_size
-d debug output compression metric debug files
-D <debug_dir> set the output directory for compression metric debug json files (defaults: $TMPDIR, /tmp)
-f <cxi_filename> filename
-p <presiso> config file
-n <workers> workers_per_node
-o <output_file> path to output the compressed and decompresed cxi, enables decompression stage
-h print this message
-v print the version information
-w <write_events> number of events to write (defaults: 0 if output_file is not set, otherwise num_events)
)";
// clang-format on

struct cmdline_args {
  std::string cxi_filename = "cxic0415_0101.cxi";
  std::string pressio_config_file = "share/blosc.json";
  std::string debug_dir = (getenv("TMPDIR") ? (std::string(getenv("TMPDIR")) + "/") : std::string("/tmp/"));
  std::string output_file;
  size_t chunk_size = 1;
  size_t start_event = 0;
  size_t write_events = std::numeric_limits<size_t>::max();
  int32_t workers_per_node = 0;
  bool debug = false;
	bool skip_copy = false;
};

using namespace std::string_literals;

cmdline_args parse_args(int argc, char* argv[]) {
  cmdline_args args;

  int opt;
  while ((opt = getopt(argc, argv, "c:dD:hvf:o:p:n:s:")) != -1) {
    switch (opt) {
      case 'c':
        args.chunk_size = atoi(optarg);
        if (args.chunk_size == 0) {
          throw std::runtime_error("invalid chunk_size"s + optarg);
        }
        break;
      case 'd':
        args.debug = true;
        break;
      case 'D':
        args.debug_dir = optarg;
        break;
      case 'f':
        args.cxi_filename = optarg;
        break;
      case 'p':
        args.pressio_config_file = optarg;
        break;
      case 'h':
        std::cout << usage << std::endl;
        exit(0);
        break;
      case 'o':
        args.output_file = optarg;
        break;
      case 'w':
        args.write_events = atoi(optarg);
        break;
			case 's':
        args.start_event = atoi(optarg);
				args.skip_copy = true;
				break;
      case 'n':
        args.workers_per_node = atoi(optarg);
        if (args.workers_per_node < 1) {
          throw std::runtime_error("invalid workers per_node"s + optarg);
        }
        break;

      case 'v':
        std::cout << ROIBIN_TEST_VERSION << std::endl;
        exit(0);
        break;
    }
  }
  if (!args.pressio_config_file.size()) {
    std::cout << "config file is required" << std::endl;
    std::cout << usage << std::endl;
    exit(1);
  }
  if (!args.cxi_filename.size()) {
    std::cout << "cxi file is required" << std::endl;
    std::cout << usage << std::endl;
    exit(1);
  }

  return args;
}

int main(int argc, char* argv[]) {
  int world_rank, world_size, per_node_rank;
  MPI_Init(&argc, &argv);
  cleanup cleanup_init([&] { MPI_Finalize(); });

  auto args = parse_args(argc, argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // create communicators of ranks within each node
  MPI_Comm per_node_comm;
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, world_rank, MPI_INFO_NULL, &per_node_comm);
  MPI_Comm_rank(per_node_comm, &per_node_rank);
  cleanup cleanup_per_node_rank([&] { MPI_Comm_free(&per_node_comm); });

  if (args.workers_per_node == 0) {
    MPI_Comm_size(per_node_comm, &args.workers_per_node);
  }

  // create communicators with 1 rank per node
  int work_rank, work_size;
  MPI_Comm work_comm;
  MPI_Comm_split(MPI_COMM_WORLD, per_node_rank < args.workers_per_node, world_rank, &work_comm);
  cleanup cleanup_work_comm([&] { MPI_Comm_free(&work_comm); });
  MPI_Comm_rank(work_comm, &work_rank);
  MPI_Comm_size(work_comm, &work_size);

  std::string write_path = args.output_file;
  if (!args.output_file.empty() && world_rank == 0 && !args.skip_copy) {
    try {
      std::cout << "started copy " << args.cxi_filename << " to " << write_path << std::endl;
      copy_file(args.cxi_filename, write_path);
      std::cout << "copied " << args.cxi_filename << " to " << write_path << std::endl;
    } catch (std::exception const& ex) {
      std::cerr << ex.what() << std::endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);

  if (per_node_rank < args.workers_per_node) {
    try {
      hid_t fapl = check_hdf5(H5Pcreate(H5P_FILE_ACCESS));
      cleanup cleanup_fapl([&] { H5Pclose(fapl); });
      check_hdf5(H5Pset_fapl_mpio(fapl, work_comm, MPI_INFO_NULL));

      hid_t cxi = check_hdf5(H5Fopen(args.cxi_filename.c_str(), H5F_ACC_RDONLY, fapl));
      cleanup cleanup_cxi([&] { H5Fclose(cxi); });

      const char* data_loc = "/entry_1/data_1/data";
      const char* peakx_loc = "/entry_1/result_1/peakXPosRaw";
      const char* peaky_loc = "/entry_1/result_1/peakYPosRaw";
      const char* npeak_loc = "/entry_1/result_1/nPeaks";

      auto data = open_dset(cxi, data_loc);
      auto posx = open_dset(cxi, peakx_loc);
      auto posy = open_dset(cxi, peaky_loc);
      auto npeaks = open_dset(cxi, npeak_loc);

      hid_t output_h5f;
      cleanup cleanup_output_h5f;
      h5dset output_data;
      if(!args.output_file.empty()) {
        output_h5f = check_hdf5(H5Fopen(write_path.c_str(), H5F_ACC_RDWR, fapl));
        cleanup_output_h5f = cleanup([=]{H5Fclose(output_h5f);});
        output_data = open_dset(output_h5f, data_loc);
      }

      // hdf5 and libpressio use opposite data ordering
      size_t num_events = data.get_dims_hsize().front();
      args.write_events = std::min(args.write_events, num_events);
      size_t max_peaks = posx.get_dims_hsize().back();
      uint64_t total_size = 0;
      uint64_t total_compressed_size = 0;
      pressio_data peaks_data = pressio_data::owning(pressio_int64_dtype, {args.chunk_size});
      pressio_data posx_data = pressio_data::owning(pressio_double_dtype, {max_peaks, args.chunk_size});
      pressio_data posy_data = pressio_data::owning(pressio_double_dtype, {max_peaks, args.chunk_size});

      auto data_lp_size = data.get_pressio_dims();
      auto data_lp_worksize = data_lp_size;
      data_lp_worksize.back() = args.chunk_size;
      pressio_data data_data = pressio_data::owning(pressio_float_dtype, data_lp_worksize);

      // prepare compressor
      std::ifstream pressio_input_file(args.pressio_config_file);
      nlohmann::json j;
      pressio_input_file >> j;
      pressio_options options_from_file(static_cast<pressio_options>(j));
      pressio library;
      pressio_compressor comp = library.get_compressor("pressio");
      comp->set_name("pressio");
      comp->set_options(options_from_file);
      comp->set_options(
          {{"pressio:metric", "composite"}, {"composite:plugins", std::vector<std::string>{"size", "time"}}});

      if (work_rank == 0) {
        std::clog << comp->get_options() << std::endl;
      }

      try {
        auto begin = std::chrono::steady_clock::now();
        uint64_t global_compress_ms = 0;
        uint64_t global_decompress_ms = 0;
        for (size_t i = args.start_event; i < num_events; i += (args.chunk_size * work_size)) {
          size_t id = i + work_rank * args.chunk_size;
          size_t read_work_items;
          if (id > num_events) {
            read_work_items = 0;
          } else if (id + args.chunk_size > num_events) {
            read_work_items = num_events - id;
          } else {
            read_work_items = args.chunk_size;
          }

          if (work_rank == 0) {
            std::clog << "processing " << i << " " << i + (args.chunk_size * work_size) << std::endl;
          }

          // read npeaks
          std::vector<hsize_t> npeaks_start{id};
          std::vector<hsize_t> npeaks_count{read_work_items};
          std::vector<size_t> peak_data_lp(npeaks_count.begin(), npeaks_count.end());
          if (read_work_items) {
            peaks_data.set_dimensions(std::move(peak_data_lp));
          }
          read(npeaks, npeaks_start, npeaks_count, peaks_data, read_work_items);
          // read posx
          std::vector<hsize_t> posx_start{id, 0};
          std::vector<hsize_t> posx_count{read_work_items, max_peaks};
          std::vector<size_t> posx_data_lp(posx_count.begin(), posx_count.end());
          if (read_work_items) {
            posx_data.set_dimensions(std::move(posx_data_lp));
          }
          read(posx, posx_start, posx_count, posx_data, read_work_items);
          // read posy
          std::vector<hsize_t> posy_start{id, 0};
          std::vector<hsize_t> posy_count{read_work_items, max_peaks};
          std::vector<size_t> posy_data_lp(posy_count.begin(), posy_count.end());
          if (read_work_items) {
            posy_data.set_dimensions(std::move(posy_data_lp));
          }
          read(posy, posy_start, posy_count, posy_data, read_work_items);

          // compute centers
          size_t peaks_in_work = 0;
          auto npeaks_ptr = static_cast<const int64_t*>(peaks_data.data());
          std::vector<size_t> peaks_to_events, to_start_of_event;
          peaks_to_events.reserve(max_peaks * read_work_items);
          to_start_of_event.reserve(read_work_items * read_work_items);
          for (size_t k = 0; k < read_work_items; ++k) {
            peaks_in_work += npeaks_ptr[k];
            for (int64_t j = 0; j < npeaks_ptr[k]; ++j) {
              peaks_to_events.push_back(k);
              to_start_of_event.push_back(j);
            }
          }
          pressio_data centers = pressio_data::owning(pressio_uint64_dtype, {3, peaks_in_work});
          auto posx_ptr = static_cast<double const*>(posx_data.data());
          auto posy_ptr = static_cast<double const*>(posy_data.data());
          auto centers_ptr = static_cast<uint64_t*>(centers.data());
          for (size_t k = 0; k < peaks_in_work; ++k) {
            centers_ptr[k * 3] =
                static_cast<size_t>(posx_ptr[peaks_to_events[k] * max_peaks + to_start_of_event[k]]);
            centers_ptr[k * 3 + 1] =
                static_cast<size_t>(posy_ptr[peaks_to_events[k] * max_peaks + to_start_of_event[k]]);
            centers_ptr[k * 3 + 2] = peaks_to_events[k];
          }

          // read data
          std::vector<hsize_t> data_start{id, 0, 0};
          std::vector<hsize_t> data_count{read_work_items, data_lp_worksize.at(1), data_lp_worksize.at(0)};
          std::vector<size_t> data_data_lp(data_count.begin(), data_count.end());
          if (read_work_items) {
            data_data.set_dimensions(std::move(data_data_lp));
          }
          read(data, data_start, data_count, data_data, read_work_items);

          uint64_t compress_time_ms = 0;
          uint64_t decompress_time_ms = 0;
          pressio_data data_comp = pressio_data::empty(pressio_byte_dtype, {});
          if (read_work_items > 0) {
            // trigger compression/decompression
            comp->set_options({{"roibin:centers", std::move(centers)}});
            auto begin_compress = std::chrono::steady_clock::now();
            if (comp->compress(&data_data, &data_comp)) {
              std::cerr << comp->error_msg();
              MPI_Abort(MPI_COMM_WORLD, comp->error_code());
            }
            auto end_compress = std::chrono::steady_clock::now();
            compress_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_compress - begin_compress).count();
          }

          if (!args.output_file.empty()) {
						try {
            size_t write_work_items;
            if (id > args.write_events) {
              write_work_items = 0;
            } else if (id + args.chunk_size > args.write_events) {
              write_work_items = num_events - id;
            } else {
              write_work_items = args.chunk_size;
            }

            pressio_data data_output = pressio_data::clone(data_data);
            if (write_work_items > 0) {
              auto begin_decompress = std::chrono::steady_clock::now();
              if (comp->decompress(&data_comp, &data_output)) {
                std::cerr << comp->error_msg();
                MPI_Abort(MPI_COMM_WORLD, comp->error_code());
              }
              auto end_decompress = std::chrono::steady_clock::now();
              decompress_time_ms =
                  std::chrono::duration_cast<std::chrono::milliseconds>(end_decompress - begin_decompress)
                      .count();
            }

            // now write out the data to save
            std::vector<hsize_t> write_data_start{id, 0, 0};
            std::vector<hsize_t> write_data_count{write_work_items, data_lp_worksize.at(1),
                                                  data_lp_worksize.at(0)};
            std::vector<size_t> write_data_data_lp(data_count.begin(), data_count.end());
            write(output_data, write_data_start, write_data_count, data_output, write_work_items);
						} catch(std::exception const& write_ex) {
							std::cerr << "write exception " << write_ex.what() << std::endl;
							MPI_Abort(MPI_COMM_WORLD, 1);
						}
          }

          // save metrics worth saving
          total_compressed_size += data_comp.size_in_bytes();
          total_size += data_data.size_in_bytes();
          if (args.debug) {
            auto metrics_results = comp->get_metrics_results();
            nlohmann::json jmr = metrics_results;
            std::stringstream ss;
            auto cxi_basename = basename(args.cxi_filename);
            auto config_basename = basename(args.pressio_config_file);
            ss << args.debug_dir << cxi_basename << '-' << config_basename << '-' << id << '-'
               << (id + read_work_items) << ".json";
            std::cerr << ss.rdbuf() << std::endl;
            std::ofstream out(ss.str());
            out << jmr;
          }
          uint64_t longest_compress_ms;
          MPI_Reduce(&compress_time_ms, &longest_compress_ms, 1, MPI_UINT64_T, MPI_MAX, 0, work_comm);
          global_compress_ms += longest_compress_ms;

          if (!args.output_file.empty()) {
            uint64_t longest_decompress_ms;
            MPI_Reduce(&decompress_time_ms, &longest_decompress_ms, 1, MPI_UINT64_T, MPI_MAX, 0, work_comm);
            global_decompress_ms += longest_decompress_ms;
          }
        }

        auto global_compressed_size = total_compressed_size;
        auto global_total_size = total_compressed_size;
        MPI_Reduce(&total_compressed_size, &global_compressed_size, 1, MPI_UINT64_T, MPI_SUM, 0, work_comm);
        MPI_Reduce(&total_size, &global_total_size, 1, MPI_UINT64_T, MPI_SUM, 0, work_comm);

        // compute global compression ratio
        if (work_rank == 0) {
          auto end = std::chrono::steady_clock::now();
          auto wallclock_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
          std::cout << "global_cr=" << global_total_size / static_cast<double>(global_compressed_size)
                    << std::endl;
          std::cout << "wallclock_ms=" << wallclock_ms << std::endl;
          std::cout << "compress_ms=" << global_compress_ms << std::endl;
          std::cout << "compress_bandwidth_GBps="
                    << global_total_size / static_cast<double>(global_compress_ms) * 1e-6 << std::endl;
          std::cout << "wallclock_bandwidth_GBps="
                    << global_total_size / static_cast<double>(wallclock_ms) * 1e-6 << std::endl;
          if (!args.output_file.empty()) {
            std::cout << "decompress_bandwidth_GBps="
                      << global_total_size / static_cast<double>(global_decompress_ms) * 1e-6 << std::endl;
          }
        }
      } catch (std::exception const& ex) {
        std::cout << "rank " << work_rank << " " << ex.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
      }

    } catch (std::exception const& ex) {
      std::cerr << ex.what() << std::endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  }

  return 0;
}
