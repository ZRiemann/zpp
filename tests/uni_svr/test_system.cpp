#include "tests.h"

#include <zpp/spdlog.h>
#include <zpp/system/system.h>

#include <cerrno>
#include <cstring>
#include <fstream>

void z::test_system() {
  z::sys::info();

  std::string path, name;
  z::sys::process_path(path, name);
  spd_inf("path[{}], name[{}]", path, name);
  auto cores = z::sys::physical_cores();
  spd_inf("physical cores: {}", cores);
  auto fs_avail = z::sys::fs_available(".");
  spd_inf("current fs available: {} bytes", fs_avail);

  // test walk_tree
  // build test tree first:
  // ./aaa/bbb/ccc.txt
  // ./aaa/ddd.txt
  // ./aaa/eee/fff/ggg.txt
  // ./hhh/
  z::sys::walk_tree("./aaa");
  z::sys::walk_tree("./hhh");

  const auto ensure_dir = [](const char *dir_path) {
    if (z::sys::mkdir_p(dir_path, 0755) != 0) {
      spd_err("mkdir_p failed: path={}, errno={}, err={}", dir_path, errno,
              std::strerror(errno));
      return false;
    }

    return true;
  };

  const auto write_text_file = [](const char *file_path, const char *text) {
    std::ofstream ofs(file_path, std::ios::out | std::ios::trunc);

    if (!ofs.is_open()) {
      spd_err("open file failed: path={}, errno={}, err={}", file_path, errno,
              std::strerror(errno));
      return false;
    }

    ofs << text;

    if (!ofs.good()) {
      spd_err("write file failed: path={}", file_path);
      return false;
    }

    return true;
  };

  bool setup_ok = true;
  setup_ok = ensure_dir("./aaa/bbb") && setup_ok;
  setup_ok = ensure_dir("./aaa/eee/fff") && setup_ok;
  setup_ok = ensure_dir("./hhh") && setup_ok;
  setup_ok = write_text_file("./aaa/bbb/ccc.txt", "ccc\n") && setup_ok;
  setup_ok = write_text_file("./aaa/ddd.txt", "ddd\n") && setup_ok;
  setup_ok = write_text_file("./aaa/eee/fff/ggg.txt", "ggg\n") && setup_ok;

  if (!setup_ok) {
    spd_err("test tree setup failed before walk_tree");
  }

  z::sys::walk_tree("./aaa", [](const z::sys::walk_entry &entry) {
    spd_inf("walk_tree callback: path={}, name={}, is_dir={}, parent_fd={}",
            entry.path, entry.name, entry.is_dir, entry.parent_fd);
    return z::sys::walk_action::del;
  });

  z::sys::walk_tree("./hhh");
}