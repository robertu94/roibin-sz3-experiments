#include "hdf5_helpers.h"

#include <libpressio_ext/cpp/printers.h>

#include <iostream>

#include "debug_helpers.h"

hid_t check_hdf5(hid_t err) {
  if (err < 0) {
    throw std::runtime_error("failed operation");
  }
  return err;
}

std::vector<hsize_t> get_space_size(hid_t space) {
  int ndims = H5Sget_simple_extent_ndims(space);
  if (ndims < 0) {
    throw std::runtime_error("failed to get dims");
  }
  std::vector<hsize_t> size(ndims);
  H5Sget_simple_extent_dims(space, size.data(), nullptr);
  return size;
}

h5dset open_dset(hid_t cxi, const char* loc) {
  hid_t data_dset = check_hdf5(H5Dopen(cxi, loc, H5P_DEFAULT));
  cleanup cleanup_data_dset([=] { H5Dclose(data_dset); });
  hid_t data_file_dspace = check_hdf5(H5Dget_space(data_dset));
  cleanup cleanup_data_file_dspace([=] { H5Sclose(data_file_dspace); });
  return h5dset{data_file_dspace, data_dset, std::move(cleanup_data_dset),
                std::move(cleanup_data_file_dspace)};
}

hid_t pressio_to_hdf5_native_type(pressio_dtype type) {
  switch (type) {
    case pressio_float_dtype:
      return H5T_NATIVE_FLOAT;
    case pressio_double_dtype:
      return H5T_NATIVE_DOUBLE;
    case pressio_int64_dtype:
      return H5T_NATIVE_INT64;
    case pressio_uint64_dtype:
      return H5T_NATIVE_UINT64;
    default:
      throw std::runtime_error("unsupported pressio type");
  }
}

void write(h5dset const& dset, std::vector<hsize_t> const& start, std::vector<hsize_t> const& count,
           pressio_data& data, size_t work_items, bool debug) {
  if(debug) {
    auto dims = data.dimensions();
    std::reverse(dims.begin(), dims.end());
    logger("pre-write: ", std::boolalpha , static_cast<bool>(work_items) , " dims=" , printer{dims}
              , " start=" , printer{start} , " count=" , printer{count}, " dtype=" , data.dtype());
  }
  hid_t file_space = check_hdf5(H5Scopy(dset.space));
  cleanup cleanup_file_space([=] { H5Sclose(file_space); });
  check_hdf5(H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start.data(),
                                 /*stride*/ nullptr, count.data(), /*block*/ nullptr));

  if(debug) logger("write-elems: data=", data.num_elements() , " space=", H5Sget_select_npoints(file_space), " work-items=" , work_items, "start=", printer{start});
  if (work_items &&
      (data.num_elements() != static_cast<size_t>(H5Sget_select_npoints(file_space)))) {
    auto dims = data.dimensions();
    std::reverse(dims.begin(), dims.end());
    std::cout << std::boolalpha << static_cast<bool>(work_items) << " dims=" << printer{dims}
              << " start=" << printer{start} << " count=" << printer{count} << std::endl;
    throw std::runtime_error("write space size does not equal buffer size " + std::to_string(work_items) +
                             " " + std::to_string(data.num_elements()) + " " +
                             std::to_string(H5Sget_select_npoints(file_space)));
  }
  hid_t mem_space = check_hdf5(H5Screate_simple(count.size(), count.data(), nullptr));
  hid_t xfer = check_hdf5(H5Pcreate(H5P_DATASET_XFER));
  H5Pset_dxpl_mpio(xfer, H5FD_MPIO_COLLECTIVE);
  cleanup cleanup_xfer([=] { H5Pclose(xfer); });
  if (debug) logger("start-write " , printer{start});
  
  check_hdf5(H5Dwrite(dset.dset, pressio_to_hdf5_native_type(data.dtype()), mem_space, file_space,
                     xfer, data.data()));
  if (debug) logger("end-write " , printer{start});
}

void read(h5dset const& dset, std::vector<hsize_t> const& start, std::vector<hsize_t> const& count,
          pressio_data& data, size_t work_items, bool debug) {
  if(debug) {
    auto dims = data.dimensions();
    std::reverse(dims.begin(), dims.end());
    logger("pre-read: ", std::boolalpha , static_cast<bool>(work_items) , " dims=" , printer{dims}
              , " start=" , printer{start} , " count=" , printer{count}, " dtype=" , data.dtype());
  }
  hid_t file_space = check_hdf5(H5Scopy(dset.space));
  cleanup cleanup_space([=] { H5Sclose(file_space); });
  check_hdf5(H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start.data(),
                                 /*stride*/ nullptr, count.data(),
                                 /*block*/ nullptr));

  if(debug) logger("read-elems: data=", data.num_elements() , " space=", H5Sget_select_npoints(file_space), " work-items=" , work_items, "start=", printer{start});
  if (work_items &&
      (data.num_elements() != static_cast<size_t>(H5Sget_select_npoints(file_space)))) {
    auto dims = data.dimensions();
    std::reverse(dims.begin(), dims.end());
    std::cout << std::boolalpha << static_cast<bool>(work_items) << " dims=" << printer{dims}
              << " start=" << printer{start} << " count=" << printer{count} << std::endl;
    throw std::runtime_error("read space size does not equal buffer size " + std::to_string(work_items) +
                             " " + std::to_string(data.num_elements()) + " " +
                             std::to_string(H5Sget_select_npoints(file_space)));
  }

  hid_t mem_space = check_hdf5(H5Screate_simple(count.size(), count.data(), nullptr));

  hid_t xfer = check_hdf5(H5Pcreate(H5P_DATASET_XFER));
  H5Pset_dxpl_mpio(xfer, H5FD_MPIO_COLLECTIVE);
  cleanup cleanup_xfer([=] { H5Pclose(xfer); });
  if(debug) {
  }
  if (debug) logger("start-read " , printer{start});
  check_hdf5(H5Dread(dset.dset, pressio_to_hdf5_native_type(data.dtype()), mem_space, file_space,
                     xfer, data.data()));
  if (debug) logger("end-read " , printer{start});
}
