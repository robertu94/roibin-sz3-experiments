#include <string>

class posix_error : public std::exception {
 public:
  posix_error(const char* msg);
  const char* what() const noexcept override;

 private:
  static std::string build_msg(const char* user_msg);
  std::string full_msg;
};

void copy_file(std::string const& source, std::string const& dest);
