#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>

struct args {
  std::chrono::seconds timeout;
  std::vector<char*> cmds;
};

std::ostream& operator<<(std::ostream& out, args const& args) {
  out << "{timeout=" << args.timeout.count() << ", cmds=[";
  for (size_t i = 0; i < (args.cmds.size() - 1); i++) {
    out << args.cmds[i] << ", ";
  }
  out << "]}";
  return out;
}
args parse_args(int argc, char* argv[]) {
  args args;

  if(argc <= 2) {
    std::cerr << "./timeout <time> cmds..." << std::endl;
    exit(1);
  } 

  args.timeout = std::chrono::seconds(std::stol(argv[1]));
  for (int i = 2; i < argc; ++i) {
   args.cmds.push_back(argv[i]);
  }
  args.cmds.push_back(nullptr);

  return args;
}

struct pipes {
  int stderr_fd[2];
  int stdout_fd[2];
};

std::string decode_status(int status) {
  if(WIFEXITED(status)) {
    return "exited " + std::to_string(WEXITSTATUS(status));
  } else if(WIFSIGNALED(status)) {
    return "signaled " + std::to_string(WTERMSIG(status));
  } else if(WIFSTOPPED(status)) {
    return "stopped";
  } else {
    return "unknown";
  }
}

void parent(pipes const& pipes, args const& args, int child) {
  //close write ends of the pipes
  close(pipes.stdout_fd[1]);
  close(pipes.stderr_fd[1]);

  int status;
  int nfds;

//  sigset_t sigmask, empty_mask;
//  sigemptyset(&sigmask);
//  sigaddset(&sigmask, SIGCHLD);
//  if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
//    perror("failed to configure sigprocmask");
//    exit(1);
//  }
//
//  struct sigaction sa;
//  sa.sa_flags = 0;
//  sa.sa_sigaction = child_sig_action;
//  sigemptyset(&empty_mask);
//  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
//    perror("failed to configure sigaction");
//  }
  
  char buf[1024*64];

  auto stderr_eof = false;
  auto stdout_eof = false;
  pid_t wpid = 0;
  do {
    nfds = 0;
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    if(!stdout_eof) {
      FD_SET(pipes.stdout_fd[0], &readfds);
      nfds = std::max(nfds, pipes.stdout_fd[0]);
    }
    if(!stderr_eof) {
      FD_SET(pipes.stderr_fd[0], &readfds);
      nfds = std::max(nfds, pipes.stderr_fd[0]);
    }
    timeval timeout;
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(args.timeout);
    timeout.tv_sec = secs.count();
    timeout.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(args.timeout - secs).count();

    auto ready = select(nfds+1, &readfds, &writefds, &exceptfds, &timeout);
    if(ready > 0) {
      if(!stdout_eof && FD_ISSET(pipes.stdout_fd[0], &readfds)) {
        int nread = read(pipes.stdout_fd[0], buf, sizeof buf);
        if(nread > 0) {
          write(STDOUT_FILENO, buf, nread);
        } else if(nread == 0) {
          stdout_eof = true;
        } else {
          perror("parent read stdout failed");
          exit(1);
        }
      }
      if(!stderr_eof && FD_ISSET(pipes.stderr_fd[0], &readfds)) {
        int nread = read(pipes.stderr_fd[0], buf, sizeof buf);
        if(nread > 0) {
          write(STDERR_FILENO, buf, nread);
        } else if(nread == 0) {
          stderr_eof = true;
        } else {
          perror("parent read stderr failed");
          exit(1);
        }
      }
    } else if(ready == 0) {
      //timeout occurred, time to stop
      std::cerr << "timeout" << std::endl;
      kill(child, SIGKILL);
      break;
    } else {
      if(errno != EINTR) {
        perror("unexpected select problem");
      }
    }
    
    errno = 0;
    wpid = waitpid(child, &status, WNOHANG);
    if(wpid == -1) {
      perror("waitpid");
    }
  } while(child != wpid || (child == wpid && !(WIFSIGNALED(status) || WIFEXITED(status))));

  int nread;
  while((nread = read(pipes.stdout_fd[0], buf, sizeof buf)) > 0) {
    write(STDOUT_FILENO, buf, nread);
  }
  while((nread = read(pipes.stderr_fd[0], buf, sizeof buf)) > 0) {
    write(STDERR_FILENO, buf, nread);
  }
  
}

void child(pipes const& pipes, args const& args) {
  if(dup2(pipes.stdout_fd[1], STDOUT_FILENO) == -1) {
    perror("child failed to dup stdout pipe");
    exit(1);
  }
  if(dup2(pipes.stderr_fd[1], STDERR_FILENO) == -1) {
    perror("child failed to dup stderr pipe");
    exit(1);
  }
  close(pipes.stderr_fd[0]);
  close(pipes.stderr_fd[1]);
  close(pipes.stdout_fd[0]);
  close(pipes.stdout_fd[1]);

  int rc = execvp(args.cmds.front(), args.cmds.data());
  if(rc) {
    perror("exec failed\n");
  }
  exit(1);
}
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
  auto args = parse_args(argc, argv);
  std::cerr << args << std::endl;

  pipes pipes;
  if(pipe(pipes.stderr_fd) == -1) {
    perror("creating stderr pipe failed\n");
    exit(2);
  }
  if(pipe(pipes.stdout_fd) == -1) {
    perror("creating stdout pipe failed\n");
    exit(2);
  }

  int proc = fork();
  switch(proc) {
    case 0:
      child(pipes, args);
      exit(0);
    case -1:
      perror("fork failed\n");
      exit(1);
    default:
      parent(pipes, args, proc);
      exit(0);
  }

  
  return 0;
}
