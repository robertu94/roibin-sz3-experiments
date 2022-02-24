#include <iostream>
#include <string>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>
#include <libpressio_ext/cpp/pressio.h>
#include <libpressio_ext/cpp/json.h>
#include "cleanup.h"
#include "roibin_test_version.h"

hid_t check_hdf5(herr_t err) {
  if(err < 0) {
    throw std::runtime_error("");
  }
  return err;
}

std::vector<hsize_t> space_get_dims(hid_t space) {
  int ndims = H5Sget_simple_extent_ndims(space);
  if(ndims < 0) {
    throw std::runtime_error("failed to get dims");
  }
  std::vector<hsize_t> size(ndims);
  H5Sget_simple_extent_dims(space, size.data(), nullptr);
  return size;
}

const std::string usage = R"(roibin_test
experimental code to test roibin_sz3

-c chunk_size
-f cxi_filename
-h print this message
-v print the version information
)";

struct cmdline_args {
  std::string cxi_filename = "cxic0415_0101.cxi";
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

      hid_t data_dset = check_hdf5(H5Dopen(cxi, "/entry_1/data", H5P_DEFAULT));
      cleanup cleanup_data_dset([&]{H5Dclose(data_dset);});
      hid_t data_file_dspace = check_hdf5(H5Dget_space(data_dset));
      cleanup cleanup_data_file_dspace([&]{H5Sclose(data_file_dspace);});

      hid_t posx_dset = check_hdf5(H5Dopen(cxi, "/entry_1/result_1/peakXPosRaw", H5P_DEFAULT));
      cleanup cleanup_posx_dset([&]{H5Dclose(data_dset);});
      hid_t posx_file_dspace = check_hdf5(H5Dget_space(posx_dset));
      cleanup cleanup_posx_file_dspace([&]{H5Sclose(posx_file_dspace);});

      hid_t posy_dset = check_hdf5(H5Dopen(cxi, "/entry_1/result_1/peakYPosRaw", H5P_DEFAULT));
      cleanup cleanup_posy_dset([&]{H5Dclose(posy_dset);});
      hid_t posy_file_dspace = check_hdf5(H5Dget_space(posy_dset));
      cleanup cleanup_posy_file_dspace([&]{H5Sclose(posy_file_dspace);});

      hid_t peaks_dset = check_hdf5(H5Dopen(cxi, "/entry_1/result_1/nPeaks", H5P_DEFAULT));
      cleanup cleanup_peaks_dset([&]{H5Dclose(data_dset);});
      hid_t peaks_file_dspace = check_hdf5(H5Dget_space(peaks_dset));
      cleanup cleanup_peaks_file_dspace([&]{H5Sclose(peaks_file_dspace);});

      hid_t numEvents_h = check_hdf5(H5Aopen_by_name(data_dset, "/entry_1/result_1/nPeaks", "numEvents", H5P_DEFAULT, H5P_DEFAULT));
      cleanup cleanup_numEvents([&]{ H5Aclose(numEvents_h); });
      int64_t numEvents = 0;
      check_hdf5(H5Aread(numEvents_h, H5T_NATIVE_INT64, &numEvents));

      hid_t maxPeaks_h = check_hdf5(H5Aopen_by_name(data_dset, "/entry_1/result_1/nPeaks", "maxPeaks", H5P_DEFAULT, H5P_DEFAULT));
      cleanup cleanup_maxPeaks([&]{ H5Aclose(maxPeaks_h); });
      int64_t maxPeaks = 0;
      check_hdf5(H5Aread(numEvents_h, H5T_NATIVE_INT64, &maxPeaks));


    } catch(std::exception const& ex) {
      std::cout << ex.what() << std::endl;
    }
  }

  MPI_Finalize();
  return 0;
}
