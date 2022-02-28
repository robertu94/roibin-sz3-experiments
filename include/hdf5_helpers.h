#ifndef HDF5_HELPERS_H_NME0K8QT
#define HDF5_HELPERS_H_NME0K8QT
#include <numeric>
#include <hdf5.h>
#include <libpressio_ext/cpp/data.h>
#include "cleanup.h"

hid_t check_hdf5(hid_t err);


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
  pressio_dtype get_pressio_dtype() const {
    hid_t dset_type = check_hdf5(H5Dget_type(dset));
    cleanup cleanup_dset_type([=]{H5Tclose(dset_type);});

    hid_t native_type = H5Tget_native_type(dset_type, H5T_DIR_DEFAULT);
    cleanup cleanup_native_type([=]{H5Tclose(native_type);});

    if(H5Tequal(native_type, H5T_NATIVE_INT8) > 0) return pressio_int8_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_INT16) > 0) return pressio_int16_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_INT32) > 0) return pressio_int32_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_INT64) > 0) return pressio_int64_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_UINT8) > 0) return pressio_uint8_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_UINT16) > 0) return pressio_uint16_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_UINT32) > 0) return pressio_uint32_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_UINT64) > 0) return pressio_uint64_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_FLOAT) > 0) return pressio_float_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_HBOOL) > 0) return pressio_bool_dtype;
    if(H5Tequal(native_type, H5T_NATIVE_DOUBLE) > 0) return pressio_double_dtype;
    throw std::runtime_error("unsupported hdf5 type");

  }
};

h5dset open_dset(hid_t cxi, const char* loc);


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

hid_t pressio_to_hdf5_native_type(pressio_dtype type);

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
          std::vector<T> work_data(num_elements);
          check_hdf5(H5Dread(data.dset, get_hdf5_native_type<T>(), H5S_ALL, file_space, /*xfer*/H5P_DEFAULT, work_data.data()));

          hid_t lcpl = check_hdf5(H5Pcreate(H5P_LINK_CREATE));
          cleanup cleanup_lcpl([&]{H5Pclose(lcpl);});
          check_hdf5(H5Pset_create_intermediate_group(lcpl, 1));

          hid_t output_data_dset = check_hdf5(H5Dcreate(output_file, dset_name, get_hdf5_native_type<T>(), output_data_dataspace, lcpl, dcpl, /*dapl*/H5P_DEFAULT));
          check_hdf5(H5Dwrite(output_data_dset, get_hdf5_native_type<T>(), H5S_ALL, H5S_ALL, H5P_DEFAULT, work_data.data()));
        }

}

void read(h5dset const& dset, std::vector<hsize_t> const& start, std::vector<hsize_t> const& count, pressio_data& data);

#endif /* end of include guard: HDF5_HELPERS_H_NME0K8QT */
