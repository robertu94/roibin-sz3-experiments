#include "file_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <string>

#include "cleanup.h"

posix_error::posix_error(const char* msg) : full_msg(build_msg(msg)) {}

const char* posix_error::what() const noexcept { return full_msg.c_str(); }

std::string posix_error::build_msg(const char* user_msg) {
  std::stringstream full_msg;
  std::array<char, 1024> buf = {'\0'};
  (void)strerror_r(errno, buf.data(), buf.size());
  full_msg << buf.data() << ": " << user_msg;
  return full_msg.str();
}

void copy_file(std::string const& src, std::string const& dest) {
  struct stat src_stat;
  int src_fd = open(src.c_str(), O_RDONLY);
  if (src_fd < 0) {
    throw posix_error("failed to stat source file");
  }
  cleanup cleanup_src([&] { close(src_fd); });

  if (fstat(src_fd, &src_stat) < 0) {
    throw posix_error("failed to stat source file");
  }

  int dest_fd = open(dest.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (dest_fd < 0) {
    throw posix_error("failed to open dest file");
  }
  cleanup cleanup_dest([&] { close(dest_fd); });
  if (ftruncate(dest_fd, src_stat.st_size)) {
    throw posix_error("failed to set the size of the file");
  }

  off_t offset = 0;
  while (offset < src_stat.st_size) {
    auto written = sendfile(dest_fd, src_fd, &offset, src_stat.st_size - offset);
    if (written == -1) {
      throw posix_error("sendfile failed");
    }
  }
  fsync(dest_fd);
}
