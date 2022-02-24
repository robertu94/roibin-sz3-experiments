#include <functional>
#include <optional>
#include <utility>
#include <stdexcept>
#include <H5Epublic.h>
class cleanup {
  public:
  template <class F>
  cleanup(F&& func): finalizer(std::move(func)) {}
  ~cleanup() {
    if(run_cleanup) {
      finalizer();
    }
  }
  cleanup(cleanup&)=delete;
  cleanup& operator=(cleanup&)=delete;
  cleanup(cleanup&& rhs) noexcept:
    run_cleanup(std::exchange(rhs.run_cleanup, false)),
    finalizer(std::exchange(rhs.finalizer, []{})) {}
  cleanup& operator=(cleanup&& rhs) noexcept{
    if(&rhs == this) return *this;
    run_cleanup = std::exchange(rhs.run_cleanup, false);
    finalizer = std::exchange(rhs.finalizer, []{});
    return *this;
  }

  private:
  bool run_cleanup = true;
  std::function<void()> finalizer;
};

