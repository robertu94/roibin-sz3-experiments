#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>
#include "cleanup.h"
#include "roibin_test_version.h"

template <class T>
struct printer {
  T const& iterable;
  decltype(std::begin(iterable)) begin() const {
    return std::begin(iterable);
  }
  decltype(std::end(iterable)) end() const {
    return std::end(iterable);
  }
};

template <class CharT, class Traits, class V>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& out, printer<V>const& p) {
  auto it = std::begin(p);
  auto end = std::end(p);

  out << '{';
  if (it != end) {
    out << *it;
    ++it;
    while(it != end) {
      out << ", " << *it;
      it++;
    }
  }
  out << '}';
  return out;
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

hid_t check_hdf5(herr_t err) {
  if(err < 0) {
    throw std::runtime_error("");
  }
  return err;
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
    try{
      auto args = parse_args(argc, argv);

      hid_t cxi = check_hdf5(H5Fopen(args.cxi_filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT));
      cleanup cleanup_cxi([&]{H5Fclose(cxi);});

      hid_t data_dset = check_hdf5(H5Dopen(cxi, "/entry_1/data_1/data", H5P_DEFAULT));
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

      //peakXPos -- numEvents x maxPeaks (double)
      //peakYPos -- numEvents x maxPeaks (double)
      //nPeaks -- numEvents (int64_t)
      //data -- numEvents x column x row (float)

      auto peaksx_dims_hsize = space_get_dims(data_file_dspace);
      auto peaksy_dims_hsize = space_get_dims(data_file_dspace);
      auto npeaks_dims_hsize = space_get_dims(data_file_dspace);
      auto data_dims_hsize = space_get_dims(data_file_dspace);

      std::cout << 
        "peaks x " << printer{peaksx_dims_hsize} <<
        "peaks y " << printer{peaksy_dims_hsize} <<
        "npeaks " << printer{npeaks_dims_hsize} <<
        "data " << printer{data_dims_hsize} <<
        std::endl;



    } catch(std::exception const& ex) {
      std::cout << ex.what() << std::endl;
    }
  return 0;
}
