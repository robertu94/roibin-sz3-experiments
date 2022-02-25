#include <iostream>
#include <string>
#include <numeric>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>
#include "cleanup.h"
#include "roibin_test_version.h"
#include "debug_helpers.h"
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
  std::string outfile = "roibin.cxi";
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
    try{
      auto args = parse_args(argc, argv);

      hid_t cxi = check_hdf5(H5Fopen(args.cxi_filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT));
      cleanup cleanup_cxi([&]{H5Fclose(cxi);});

      const char* data_loc="/entry_1/data_1/data";
      const char* peakx_loc="/entry_1/result_1/peakXPosRaw";
      const char* peaky_loc="/entry_1/result_1/peakYPosRaw";
      const char* npeak_loc="/entry_1/result_1/nPeaks";

      auto data = open_dset(cxi, data_loc);
      auto posx = open_dset(cxi, peakx_loc);
      auto posy = open_dset(cxi, peaky_loc);
      auto npeaks = open_dset(cxi, npeak_loc);

      hsize_t numEvents = get_attribute<int64_t>(cxi, npeak_loc, "numEvents");
      hsize_t maxPeaks = get_attribute<int64_t>(cxi, npeak_loc, "maxPeaks");

      //peakXPos -- numEvents x maxPeaks (double)
      //peakYPos -- numEvents x maxPeaks (double)
      //nPeaks -- numEvents (int64_t)
      //data -- numEvents x column x row (float)
      if(numEvents < args.chunk_size) {
        throw std::runtime_error("too few events");
      }

      auto peaksx_dims_hsize = posx.get_dims_hsize();
      auto peaksy_dims_hsize = posy.get_dims_hsize();
      auto npeaks_dims_hsize = npeaks.get_dims_hsize();
      auto data_dims_hsize =  data.get_dims_hsize();
      std::cout << 
        "peaks x " << printer{peaksx_dims_hsize} <<
        "peaks y " << printer{peaksy_dims_hsize} <<
        "npeaks " << printer{npeaks_dims_hsize} <<
        "data " << printer{data_dims_hsize} <<
        std::endl;


      auto work_peaksx_dims_hsize = std::vector<hsize_t>{args.chunk_size, maxPeaks};
      auto work_peaksy_dims_hsize = std::vector<hsize_t>{static_cast<hsize_t>(args.chunk_size), maxPeaks};
      auto work_npeaks_dims_hsize = std::vector<hsize_t>{static_cast<hsize_t>(args.chunk_size)};
      auto work_data_dims_hsize = std::vector<hsize_t>{static_cast<hsize_t>(args.chunk_size), data_dims_hsize[1], data_dims_hsize[2]};

      auto chunk_data_hsize = std::vector<hsize_t>{static_cast<hsize_t>(1), data_dims_hsize[1], data_dims_hsize[2]};
      auto chunk_peak_hsize = std::vector<hsize_t>{static_cast<hsize_t>(1), maxPeaks};

      {
        hid_t output_file = check_hdf5(H5Fcreate(args.outfile.c_str(), 0, /*create*/H5P_DEFAULT, /*access*/H5P_DEFAULT));
        cleanup output_file_cleanup([&]{H5Fclose(output_file);});

        hid_t data_dcpl = check_hdf5(H5Pcreate(H5P_DATASET_CREATE));
        cleanup cleanup_dapl([&]{H5Pclose(data_dcpl);});
        check_hdf5(H5Pset_chunk(data_dcpl, chunk_data_hsize.size(), chunk_data_hsize.data()));
        check_hdf5(H5Pset_deflate(data_dcpl, 6));

        hid_t peak_dcpl = check_hdf5(H5Pcreate(H5P_DATASET_CREATE));
        cleanup cleanup_peak_dapl([&]{H5Pclose(peak_dcpl);});
        check_hdf5(H5Pset_chunk(peak_dcpl, chunk_peak_hsize.size(), chunk_peak_hsize.data()));
        check_hdf5(H5Pset_deflate(peak_dcpl, 6));


        std::clog << "copy data" << std::endl;
        copy<float>(data, work_data_dims_hsize, data_dcpl, output_file, data_loc);
        std::clog << "copy peakx" << std::endl;
        copy<double>(posx, work_peaksx_dims_hsize, peak_dcpl, output_file, peakx_loc);
        std::clog << "copy peaky" << std::endl;
        copy<double>(posy, work_peaksy_dims_hsize, peak_dcpl, output_file, peaky_loc);
        std::clog << "copy npeak" << std::endl;
        copy<int64_t>(npeaks, work_npeaks_dims_hsize, H5P_DEFAULT, output_file, npeak_loc);
        std::clog << "done" << std::endl;
      }


    } catch(std::exception const& ex) {
      std::cout << ex.what() << std::endl;
    }
  return 0;
}
