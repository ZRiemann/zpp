#pragma once

#include <zpp/namespace.h>
#include <zpp/system/os.h>

#include <cstdint>
#include <functional>
#include <string>

#if (Z_OS_WINDOWS) || (Z_OS_UNIX)
NSB_ZPP
class sys {
public:
  sys();
  ~sys();
  /**
   * @brief 显示操作系统相关信息
   */
  static void info();
  /**
   * @brief 获取当前进程的路经和名称
   */
  static void process_path(std::string &path, std::string &name);

  static std::size_t physical_cores();
  static std::uint64_t fs_available(const char *path);
  static std::uint64_t fs_available(const std::string &path) {
    return fs_available(path.c_str());
  }

  static const std::string &uuid();
  static int rename(const char *oldpath, const char *newpath);
  static int mkdir_p(const char *path, mode_t mode = 0755);

public: // walk_tree
  enum class walk_action {
    // 文件：删除当前文件，然后继续。
    // 目录：进入目录递归处理，之后若为空则删除目录，然后继续。
    // 用于删除目录树。
    del,
    // 文件：保留当前文件，然后继续。
    // 目录：进入目录递归处理，之后保留目录，然后继续。
    // 用于遍历目录树。
    enter,
    // 文件：保留当前文件，然后继续。
    // 目录：跳过整个目录树，不进入、不删除，然后继续。
    skip,
    // 文件/目录：保留当前 entry，不进入、不删除，直接结束遍历。
    done
  };

  struct walk_entry {
    // 当前 entry 的完整路径。
    // 仅在当前回调调用期间有效，如需保存，请复制为 std::string。
    std::string_view path;

    // 当前 entry 的 basename。
    // 仅在当前回调调用期间有效，如需保存，请复制为 std::string。
    std::string_view name;

    // 当前 entry 是否为目录。
    bool is_dir = false;

    // 当前 entry 的父目录 fd。
    //
    // 对普通子项：
    //   parent_fd >= 0，可用于 openat(parent_fd, name, ...)
    //
    // 对 walk_tree(path) 的 root entry：
    //   parent_fd == -1
    int parent_fd = -1;
  };

  using walk_callback = std::function<walk_action(const walk_entry &entry)>;

  // 核心接口：
  //
  // cb 为空时，默认行为是 walk_action::del，
  // 即删除整个目录树，包括 path 自身。
  //
  // 如果只想遍历、不删除：
  //   walk_tree(path, [](const walk_entry&) {
  //       return walk_action::enter;
  //   });
  static int walk_tree(const char *path, walk_callback cb = {});
};
NSE_ZPP
#else
#error not support
#endif