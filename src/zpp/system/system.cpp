#include <zpp/system/system.h>

#include <cstring>
#include <stdlib.h>

#include <thread>
#include <zpp/namespace.h>
#include <zpp/spdlog.h>

#if Z_OS_WINDOWS
#include <windows.h>

#elif Z_OS_UNIX
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#else
#error not support
#endif

namespace {

enum class walk_status { ok, stopped, failed };

using walk_action = z::sys::walk_action;
using walk_entry = z::sys::walk_entry;
using walk_callback = z::sys::walk_callback;

bool is_dot_or_dotdot(const char *name) {
  return name[0] == '.' &&
         (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

std::string join_path(const std::string &parent, const char *name) {
  if (parent.empty()) {
    return std::string(name);
  }

  if (parent == "/") {
    return "/" + std::string(name);
  }

  if (parent.back() == '/') {
    return parent + name;
  }

  return parent + "/" + name;
}

struct normalized_path {
  std::string full;
  std::string name;
};

normalized_path split_normalized_path(const char *path) {
  normalized_path out;

  if (!path) {
    return out;
  }

  out.full = path;

  while (out.full.size() > 1 && out.full.back() == '/') {
    out.full.pop_back();
  }

  if (out.full.empty()) {
    return out;
  }

  if (out.full == "/") {
    out.name = "/";
    return out;
  }

  auto pos = out.full.find_last_of('/');

  if (pos == std::string::npos) {
    out.name = out.full;
  } else {
    out.name = out.full.substr(pos + 1);
  }

  return out;
}

bool is_forbidden_root(const std::string &path) {
  return path.empty() || path == "/" || path == "." || path == "..";
}

bool is_non_empty_dir_error(int err) {
  return err == ENOTEMPTY || err == EEXIST;
}

int open_dir_nofollow(const char *path) {
  return ::open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
}

int open_dir_nofollow_at(int dfd, const char *name) {
  return ::openat(dfd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
}

walk_action decide(const walk_callback &cb, const walk_entry &entry) {
  if (!cb) {
    return walk_action::del;
  }

  return cb(entry);
}

walk_status walk_tree_fd(int dfd, const std::string &cur_path,
                         const walk_callback &cb);

walk_status process_entry(int parent_dfd, const char *name,
                          const std::string &full_path, bool is_dir,
                          const walk_callback &cb) {
  walk_entry entry{.path = std::string_view(full_path),
                   .name = std::string_view(name),
                   .is_dir = is_dir,
                   .parent_fd = parent_dfd};

  const walk_action action = decide(cb, entry);

  switch (action) {
  case walk_action::done:
    return walk_status::stopped;

  case walk_action::skip:
    return walk_status::ok;

  case walk_action::enter:
  case walk_action::del:
    break;
  }

  if (!is_dir) {
    if (action == walk_action::del) {
      if (::unlinkat(parent_dfd, name, 0) != 0) {
        return walk_status::failed;
      }
    }

    return walk_status::ok;
  }

  int sub = open_dir_nofollow_at(parent_dfd, name);
  if (sub < 0) {
    return walk_status::failed;
  }

  walk_status st = walk_tree_fd(sub, full_path, cb);

  if (st == walk_status::failed) {
    return walk_status::failed;
  }

  if (st == walk_status::stopped) {
    return walk_status::stopped;
  }

  if (action == walk_action::del) {
    if (::unlinkat(parent_dfd, name, AT_REMOVEDIR) != 0) {
      if (!is_non_empty_dir_error(errno)) {
        return walk_status::failed;
      }
    }
  }

  return walk_status::ok;
}

walk_status walk_tree_fd(int dfd, const std::string &cur_path,
                         const walk_callback &cb) {
  DIR *dir = ::fdopendir(dfd);

  if (!dir) {
    int saved = errno;
    ::close(dfd);
    errno = saved;
    return walk_status::failed;
  }

  walk_status result = walk_status::ok;
  int saved_errno = 0;

  while (true) {
    errno = 0;
    struct dirent *ent = ::readdir(dir);

    if (!ent) {
      if (errno != 0) {
        result = walk_status::failed;
        saved_errno = errno;
      }
      break;
    }

    const char *name = ent->d_name;

    if (is_dot_or_dotdot(name)) {
      continue;
    }

    bool is_dir = false;

    if (ent->d_type == DT_DIR) {
      is_dir = true;
    } else if (ent->d_type == DT_UNKNOWN) {
      struct stat st{};

      if (::fstatat(::dirfd(dir), name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        result = walk_status::failed;
        saved_errno = errno;
        break;
      }

      is_dir = S_ISDIR(st.st_mode);
    }

    std::string full_path = join_path(cur_path, name);

    result = process_entry(::dirfd(dir), name, full_path, is_dir, cb);

    if (result != walk_status::ok) {
      if (result == walk_status::failed) {
        saved_errno = errno;
      }
      break;
    }
  }

  if (::closedir(dir) != 0 && result == walk_status::ok) {
    result = walk_status::failed;
    saved_errno = errno;
  }

  if (result == walk_status::failed && saved_errno != 0) {
    errno = saved_errno;
  }

  return result;
}

int ensure_dir(const char *path, mode_t mode) {
  if (::mkdir(path, mode) == 0) {
    return 0;
  }

  if (errno != EEXIST) {
    return -1;
  }

  struct stat st{};

  if (::stat(path, &st) != 0) {
    return -1;
  }

  if (!S_ISDIR(st.st_mode)) {
    errno = ENOTDIR;
    return -1;
  }

  return 0;
}

std::string read_file_trimmed(const char *path) {
  int fd = ::open(path, O_RDONLY | O_CLOEXEC);

  if (fd < 0) {
    return {};
  }

  char buf[256];

  ssize_t n;
  do {
    n = ::read(fd, buf, sizeof(buf) - 1);
  } while (n < 0 && errno == EINTR);

  int saved = errno;
  ::close(fd);
  errno = saved;

  if (n <= 0) {
    return {};
  }

  buf[n] = '\0';

  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' ||
                   buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
    buf[--n] = '\0';
  }

  return std::string(buf, static_cast<std::size_t>(n));
}

std::string read_uuid_once() {
  std::string uuid = read_file_trimmed("/sys/class/dmi/id/product_uuid");

  if (!uuid.empty()) {
    return uuid;
  }

  return read_file_trimmed("/etc/machine-id");
}

} // namespace

USE_ZPP

sys::sys() {}
sys::~sys() {}

std::size_t sys::physical_cores() {
  return std::max(1u, std::thread::hardware_concurrency());
}

struct OSInfo {
  std::string name;     // OS 名称/内核名
  std::string version;  // 版本/内核版本
  std::string arch;     // 架构
  std::string hostname; // 主机名
};

#if Z_OS_WINDOWS
typedef LONG(WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

void sys::info() {
  OSInfo info;

  // 主机名
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
    char hn[256] = {0};
    DWORD sz = sizeof(hn);
    if (GetComputerNameA(hn, &sz))
      info.hostname = hn;
    WSACleanup();
  }

  // 架构
  SYSTEM_INFO si{};
  GetNativeSystemInfo(&si);
  switch (si.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    info.arch = "x64";
    break;
  case PROCESSOR_ARCHITECTURE_ARM64:
    info.arch = "arm64";
    break;
  case PROCESSOR_ARCHITECTURE_ARM:
    info.arch = "arm";
    break;
  case PROCESSOR_ARCHITECTURE_INTEL:
    info.arch = "x86";
    break;
  default:
    info.arch = "unknown";
    break;
  }

  // 名称 + 版本（简化）
  info.name = "Windows";
  // 尝试用 RtlGetVersion 获取真实版本（不受清单/兼容性影响）
  HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
  if (hNt) {
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        GetProcAddress(hNt, "RtlGetVersion"));
    if (rtlGetVersion) {
      RTL_OSVERSIONINFOW rovi{};
      rovi.dwOSVersionInfoSize = sizeof(rovi);
      if (rtlGetVersion(&rovi) == 0) {
        info.version = std::to_string(rovi.dwMajorVersion) + "." +
                       std::to_string(rovi.dwMinorVersion) + "." +
                       std::to_string(rovi.dwBuildNumber);
        spd_inf("system infomation:\nname: {}\nversion: {}\narch: "
                "{}\nhostname: {}\n",
                info.name, info.version, info.arch, info.hostname);
        return;
      }
    }
  }
  // 兜底：GetVersionEx 已弃用且可能返回错误版本，这里不再调用。作为占位：
  info.version = "unknown";
}

std::uint64_t sys::fs_available(const char *path) {
  ULARGE_INTEGER freeBytesAvailable;
  if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, nullptr, nullptr)) {
    return freeBytesAvailable.QuadPart;
  }
  return 0; // 获取失败
}

#elif Z_OS_UNIX
void sys::info() {
  OSInfo info;
  struct utsname u{};
  if (uname(&u) == 0) {
    info.name = u.sysname; // Linux, Darwin, FreeBSD...
    info.version = std::string(u.release) + " (" + u.version + ")";
    info.arch = u.machine; // x86_64, aarch64/arm64...
  } else {
    info.name = "Unknown Unix";
  }
  // 主机名
  char hn[256] = {0};
  if (gethostname(hn, sizeof(hn)) == 0)
    info.hostname = hn;
  spd_inf("system infomation:\nname: {}\nversion: {}\narch: {}\nhostname: {}\n",
          info.name, info.version, info.arch, info.hostname);
}

std::uint64_t sys::fs_available(const char *path) {
  struct statvfs stat{};
  if (statvfs(path, &stat) != 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(stat.f_bsize) * stat.f_bavail;
}
const std::string &sys::uuid() {
  static const std::string cached = read_uuid_once();
  return cached;
}

int sys::walk_tree(const char *path, walk_callback cb) {
  if (!path || !*path) {
    errno = EINVAL;
    return -1;
  }

  normalized_path root = split_normalized_path(path);

  if (is_forbidden_root(root.full)) {
    errno = EINVAL;
    return -1;
  }

  walk_entry root_entry{.path = std::string_view(root.full),
                        .name = std::string_view(root.name),
                        .is_dir = true,
                        .parent_fd = -1};

  const walk_action action = decide(cb, root_entry);

  switch (action) {
  case walk_action::done:
  case walk_action::skip:
    return 0;

  case walk_action::enter:
  case walk_action::del:
    break;
  }

  int dfd = open_dir_nofollow(root.full.c_str());

  if (dfd < 0) {
    return -1;
  }

  walk_status st = walk_tree_fd(dfd, root.full, cb);

  if (st == walk_status::failed) {
    return -1;
  } else if (st == walk_status::stopped) {
    return 0;
  }

  if (action == walk_action::del) {
    if (::rmdir(root.full.c_str()) != 0) {
      if (!is_non_empty_dir_error(errno)) {
        return -1;
      }
    }
  }

  return 0;
}

int sys::rename(const char *oldpath, const char *newpath) {
  if (!oldpath || !*oldpath || !newpath || !*newpath) {
    errno = EINVAL;
    return -1;
  }

  return ::rename(oldpath, newpath);
}

int sys::mkdir_p(const char *path, mode_t mode) {
  if (!path || !*path) {
    errno = EINVAL;
    return -1;
  }

  char *tmp = ::strdup(path);

  if (!tmp) {
    return -1;
  }

  std::size_t len = ::strlen(tmp);

  while (len > 1 && tmp[len - 1] == '/') {
    tmp[--len] = '\0';
  }

  if (::strcmp(tmp, "/") == 0) {
    ::free(tmp);
    return 0;
  }

  for (char *p = tmp + 1; *p; ++p) {
    if (*p != '/') {
      continue;
    }

    *p = '\0';

    if (tmp[0] != '\0') {
      if (ensure_dir(tmp, mode) != 0) {
        int saved = errno;
        ::free(tmp);
        errno = saved;
        return -1;
      }
    }

    *p = '/';

    while (p[1] == '/') {
      ++p;
    }
  }

  if (ensure_dir(tmp, mode) != 0) {
    int saved = errno;
    ::free(tmp);
    errno = saved;
    return -1;
  }

  ::free(tmp);
  return 0;
}

#else
#endif

void sys::process_path(std::string &path, std::string &name) {
  const int max_size = 256;
  if (path.capacity() < max_size) {
    path.resize(max_size, 0);
  }
#ifdef _WIN32
  int len = GetModuleFileNameA(NULL, (char *)path.data(), max_size);
  if (0 == len) {
    // return GetLastError();
    return;
  }
  size_t pos = path.rfind('\\');
#elif (defined __unix__)
  ssize_t len = readlink("/proc/self/exe", (char *)path.data(), max_size);
  if (-1 == len) {
    // return errno;
    return;
  }
  size_t pos = path.rfind('/');
#else
  return;
#endif
  if (std::string::npos == pos) {
    return;
  }
  name = path.substr(pos + 1, strlen(path.c_str()) - pos - 1);
  path = path.substr(0, pos + 1);
}