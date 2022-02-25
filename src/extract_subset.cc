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
template <class T>
printer(T) -> printer<T>;

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

hid_t check_hdf5(hid_t err) {
  if(err < 0) {
    throw std::runtime_error("failed operation");
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

struct h5dset {
  hid_t space, dset;
  cleanup cleanup_space, cleanup_dset;

  std::vector<hsize_t> get_dims_hsize() const {
    int ndims = H5Sget_simple_extent_ndims(space);
    if(ndims < 0) {
      throw std::runtime_error("failed to get dims");
    }
    std::vector<hsize_t> size(ndims);
    H5Sget_simple_extent_dims(space, size.data(), nullptr);
    return size;
  }
  std::vector<size_t> get_pressio_dims() const {
    auto hdims = get_dims_hsize();
    return std::vector<size_t>(hdims.rbegin(), hdims.rend());
  }
};

h5dset open_dset(hid_t cxi, const char* loc) {
    hid_t data_dset = check_hdf5(H5Dopen(cxi, loc, H5P_DEFAULT));
    cleanup cleanup_data_dset([=]{H5Dclose(data_dset);});
    hid_t data_file_dspace = check_hdf5(H5Dget_space(data_dset));
    cleanup cleanup_data_file_dspace([=]{H5Sclose(data_file_dspace);});
    return h5dset {
      data_file_dspace,
      data_dset,
      std::move(cleanup_data_dset),
      std::move(cleanup_data_file_dspace)
    };
}


template <class T>
hid_t get_hdf5_native_type() {
  if(std::is_same<T,int64_t>::value) {
    return H5T_NATIVE_INT64;
  } else if(std::is_same<T,uint64_t>::value) {
    return H5T_NATIVE_UINT64;
  } else if(std::is_same<T,float>::value) {
    return H5T_NATIVE_FLOAT;
  } else if(std::is_same<T,double>::value) {
    return H5T_NATIVE_DOUBLE;
  } else {
    throw std::runtime_error("unsupported type");
  }
}

template <class T>
T get_attribute(hid_t file, const char* dset_loc, const char* attrib_loc) {
      hid_t attr_hid = check_hdf5(H5Aopen_by_name(file, dset_loc, attrib_loc, H5P_DEFAULT, H5P_DEFAULT));
      cleanup cleanup_numEvents([=]{ H5Aclose(attr_hid); });
      T attr = 0;
      check_hdf5(H5Aread(attr_hid, get_hdf5_native_type<T>(), &attr));
      return attr;
}

template <class T>
void copy(
        h5dset const& data,
        std::vector<hsize_t> const& count,
        hid_t dcpl,
        hid_t output_file,
        const char* dset_name
      ) {
        {
          hid_t output_data_dataspace = check_hdf5(H5Screate_simple(count.size(), count.data() , nullptr));
          cleanup output_space_cleanup([&]{H5Sclose(output_data_dataspace);});


          std::vector<hsize_t> start(data.get_dims_hsize().size(), 0);
          hid_t file_space = check_hdf5(H5Scopy(data.space));
          cleanup cleanup_filespace([=]{H5Sclose(file_space);});
          H5Sselect_hyperslab(
              file_space, H5S_SELECT_SET,
              start.data(),
              /*stride*/nullptr,
              count.data(),
              /*block*/nullptr
              );

          size_t num_elements = std::accumulate(count.begin(), count.end(), hsize_t{1}, std::multiplies<>{});
          std::vector<float> work_data(num_elements);
          check_hdf5(H5Dread(data.dset, H5T_NATIVE_FLOAT, H5S_ALL, file_space, /*xfer*/H5P_DEFAULT, work_data.data()));

          hid_t lcpl = check_hdf5(H5Pcreate(H5P_LINK_CREATE));
          cleanup cleanup_lcpl([&]{H5Pclose(lcpl);});
          check_hdf5(H5Pset_create_intermediate_group(lcpl, 1));

          hid_t output_data_dset = check_hdf5(H5Dcreate(output_file, dset_name, get_hdf5_native_type<T>(), output_data_dataspace, lcpl, dcpl, /*dapl*/H5P_DEFAULT));
          check_hdf5(H5Dwrite(output_data_dset, get_hdf5_native_type<T>(), H5S_ALL, H5S_ALL, H5P_DEFAULT, work_data.data()));
        }

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

      auto peaksx_dims_hsize = posx.get_dims_hsize();
      auto peaksy_dims_hsize = posy.get_dims_hsize();
      auto npeaks_dims_hsize = npeaks.get_dims_hsize();
      auto data_dims_hsize =  data.get_dims_hsize();

      auto work_peaksx_dims_hsize = std::vector<hsize_t>{args.chunk_size, maxPeaks};
      auto work_peaksy_dims_hsize = std::vector<hsize_t>{static_cast<hsize_t>(args.chunk_size), maxPeaks};
      auto work_npeaks_dims_hsize = std::vector<hsize_t>{static_cast<hsize_t>(args.chunk_size)};
      auto work_data_dims_hsize = std::vector<hsize_t>{static_cast<hsize_t>(args.chunk_size), data_dims_hsize[1], data_dims_hsize[2]};

      auto chunk_data_hsize = std::vector<hsize_t>{static_cast<hsize_t>(1), data_dims_hsize[1], data_dims_hsize[2]};
      auto chunk_peak_hsize = std::vector<hsize_t>{static_cast<hsize_t>(1), maxPeaks};

      std::cout << 
        "peaks x " << printer{peaksx_dims_hsize} <<
        "peaks y " << printer{peaksy_dims_hsize} <<
        "npeaks " << printer{npeaks_dims_hsize} <<
        "data " << printer{data_dims_hsize} <<
        std::endl;

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
