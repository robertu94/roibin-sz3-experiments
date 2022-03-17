#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <string>

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

struct args {
  std::string cxi_filename;
  std::string output_dir;
};

args parse_args(int argc, char* argv[]) {
  args args;

  int opt;
  while ((opt = getopt(argc, argv, "f:o:")) != -1) {
    switch (opt) {
      case 'f':
        args.cxi_filename = optarg;
        break;
      case 'o':
        args.output_dir = optarg;
        break;
      case 'h':
        std::cout << "test the use of sendfile" << std::endl;
        std::cout << "-f <input_file>" << std::endl;
        std::cout << "-o <output_dir>" << std::endl;
        exit(0);
    }
  }

  return args;
}

template <class T>
T check_impl(T ret, const char* func, const char* file, int line) {
  if (ret < 0) {
    std::string msg = file;
    msg += ":";
    msg += std::to_string(line);
    msg += " ";
    msg += func;

    perror(msg.c_str());
    exit(1);
  }
  return ret;
}
#define check(...) check_impl((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

int main(int argc, char* argv[]) {
  args args = parse_args(argc, argv);
  std::string write_path = args.output_dir + '/' + basename(args.cxi_filename);
  struct stat s;
  int fd = check(open(args.cxi_filename.c_str(), O_RDONLY));
  check(fstat(fd, &s));

  int dest = check(open(write_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR));
  ftruncate(dest, s.st_size);

  off_t offset = 0;
  while (offset < s.st_size) {
    auto written = sendfile(dest, fd, &offset, s.st_size - offset);
    std::cout << "wrote " << written << " bytes" << std::endl;
  }
  fsync(dest);

  close(fd);
  close(dest);
}
