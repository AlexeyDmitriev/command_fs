#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <array>
#include <string>
#include <iostream>
#include <vector>
#include <sys/stat.h>

std::string root;
std::vector<std::string> command;

struct ChildProcess {
  ChildProcess(pid_t pid): pid(pid) {}

  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;

  ~ChildProcess() {
    if (pid != 0) {
      waitpid(pid, nullptr, 0);
    }
  }

private:
  pid_t pid;
};

struct CommandResult {
  int fd;
  ChildProcess child_process;
};

CommandResult run_command(const std::vector<std::string>& arguments) {
  std::array<int, 2> pipe_ends;
  if (pipe(pipe_ends.data()) == -1) {
    return {-errno, {0}};
  }
  auto pid = fork();
  if (pid == -1) {
    return {-errno, {0}};
  }
  if (pid == 0) {
    dup2(pipe_ends[1], STDOUT_FILENO);
    close(STDIN_FILENO);
    close(pipe_ends[0]);
    std::vector<const char*> c_style_arguments;
    c_style_arguments.reserve(arguments.size() + 1);
    std::transform(
        arguments.begin(),
        arguments.end(),
        std::back_inserter(c_style_arguments),
        [](const std::string& argument) {
          return argument.data();
        }
    );
    for (auto x: c_style_arguments) {
      std::cerr << x << "\n";
    }
    c_style_arguments.push_back(nullptr);
    if (::execvp(c_style_arguments[0], const_cast<char* const*>(c_style_arguments.data())) == -1) {
      exit(-errno);
    }
    __builtin_unreachable();
  } else {
    close(pipe_ends[1]);
    return CommandResult{
        pipe_ends[0],
        ChildProcess{pid}
    };
  }
}

CommandResult run_command(const std::string& full_path) {
  auto command_with_file = command;
  command_with_file.push_back(full_path);
  return run_command(command_with_file);
}

size_t get_length(int fd) {
  std::array<char, 4096> ignored_buffer;
  size_t result = 0;
  while (true) {
    auto bytes_read = read(fd, ignored_buffer.data(), ignored_buffer.size());
    if (bytes_read == 0) {
      return result;
    }
    result += bytes_read;
  }
}

static int xmp_getattr(
    const char *path,
    struct stat *stbuf
) {
  auto full_path = root + path;
  std::cerr << "XXX. getattr: " << path << std::endl;
  int res = lstat(full_path.c_str(), stbuf);
  if (S_ISREG(stbuf->st_mode)) {
    auto [fd, file_to_close] = run_command(full_path);
    if (fd == -1) {
      return -errno;
    }
    stbuf->st_size = get_length(fd);
    close(fd);
  }
  if (res == -1) {
    std::cerr << "XXX. errno: " << errno << std::endl;
    return -errno;
  }

  std::cerr << "XXX. ok" << std::endl;

  return 0;
}

static int xmp_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t,
    fuse_file_info*
) {
  auto full_path = root + path;
  DIR* dp = opendir(full_path.c_str());
  if (dp == NULL)
    return -errno;

  while (dirent* de = readdir(dp)) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buf, de->d_name, &st, 0)) {
      break;
    }
  }

  closedir(dp);
  return 0;
}

static int xmp_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    fuse_file_info*
) {
  auto full_path = root + path;

  auto [fd, file_to_close] = run_command(full_path);
  if (fd == -1) {
    return -errno;
  }
  std::array<char, 4096> ignored_buffer;
  while (offset > 0) {
    auto bytes_read = read(fd, ignored_buffer.data(), std::min<std::size_t>(size, ignored_buffer.size()));
    if (bytes_read == -1) {
      return -errno;
    }
    offset -= bytes_read;
  }
  auto res = read(fd, buf, size);
  if (res == -1) {
    res = -errno;
  }
  close(fd);
  return res;
}

static const struct fuse_operations command_fs_operations = {
    .getattr = xmp_getattr,
    .read = xmp_read,
    .readdir = xmp_readdir,
};


int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: command_fs mount_point root [include_path [include_path ... ]]";
    return 1;
  }
  umask(0);
  char* args_to_fuse[] = {
      argv[0],
      argv[1],
      nullptr
  };
  root = argv[2];
  command = {"/Users/alexeyd/CLionProjects/command_fs/extract_includes.py"};
  command.insert(command.end(), argv + 3, argv + argc);
  return fuse_main(
      static_cast<int>(sizeof(args_to_fuse) / sizeof(args_to_fuse[0])) - 1,
      &args_to_fuse[0],
      &command_fs_operations,
      nullptr
  );
}

