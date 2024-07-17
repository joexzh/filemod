//
// Created by Joe Tse on 11/26/23.
//
#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "CLI/Validators.hpp"
#include "filemod.h"
#include "src/fs.h"
#include "src/utils.h"

inline auto create_fm() {
  return filemod::FileMod(
      std::make_unique<filemod::FS>(filemod::get_config_path()),
      std::make_unique<filemod::Db>(filemod::get_db_path()));
}

int main(int argc, char **argv) {
  if (!filemod::real_effective_user_match()) {
    std::cerr << "suid is not supported!\n" << std::endl;
    return 1;
  }

  CLI::App app{"filemod is file replacement manager."};
  app.set_help_all_flag("--help-all");

  class MyFormatter : public CLI::Formatter {
   public:
    MyFormatter() : Formatter() {}

    std::string make_option_opts(const CLI::Option *) const override {
      return "";
    }
  };

  auto fmt = std::make_shared<MyFormatter>();
  app.formatter(fmt);

  auto name_validator = [](const std::string &str) -> std::string {
    if (str.empty() || str[0] == '-' ||
        std::string_view(str).substr(0, 2) == "--") {
      const char prefix[]{"invalid str: "};
      std::string msg;
      msg.reserve(sizeof(prefix) - 1 + str.size());
      msg += prefix;
      msg += str;
      return msg;
    }
    return {};
  };

  app.require_subcommand(1);

  int64_t target_id{-1};
  std::vector<int64_t> target_ids;
  std::vector<int64_t> mod_ids;
  std::string dir;

  auto add = app.add_subcommand("add", "add target or mod to management");
  add->add_option("--tdir", dir, "target directory")
      ->take_last()
      ->check(CLI::ExistingDirectory);
  auto mdir_opt =
      add->add_option("--mdir", dir, "mod directory")->excludes("--tdir");
  add->add_option("--t", target_id, "target id")
      ->excludes("--tdir")
      ->needs("--mdir")
      ->take_last()
      ->check(name_validator);
  mdir_opt->needs("--t")->take_last()->check(CLI::ExistingDirectory);

  auto ins = app.add_subcommand("install", "install mod to target directory");
  ins->add_option("--t", target_id, "target id")
      ->take_last()
      ->check(name_validator);
  ins->add_option("--mdir", dir, "mod directory")
      ->needs("--t")
      ->take_last()
      ->check(CLI::ExistingDirectory);
  ins->add_option("--m", mod_ids, "mod ids")
      ->excludes("--t", "--mdir")
      ->take_all()
      ->check(name_validator);

  auto uns =
      app.add_subcommand("uninstall", "uninstall mod from target directory");
  uns->add_option("--t", target_id, "target id")
      ->take_last()
      ->check(name_validator);
  uns->add_option("--m", mod_ids, "mod ids")
      ->excludes("--t")
      ->take_all()
      ->check(name_validator);

  auto rmv =
      app.add_subcommand("remove", "remove target or mod from management");
  rmv->add_option("--t", target_id, "target id")
      ->take_last()
      ->check(name_validator);
  rmv->add_option("--m", mod_ids, "mod ids")
      ->excludes("--t")
      ->take_all()
      ->check(name_validator)
      ->take_last();

  auto lst = app.add_subcommand("list", "list managed targets and mods");
  lst->add_option("--t", target_ids, "target ids")
      ->take_all()
      ->check(name_validator);
  lst->add_option("--m", mod_ids, "mod ids")
      ->excludes("--t")
      ->take_all()
      ->check(name_validator);

  filemod::result_base ret{.success = true};

  add->callback([&]() {
    std::cout << "add callback: target id: " << target_id << " dir: " << dir
              << "\n";
    auto fm = create_fm();
    if (!dir.empty() && target_id > -1) {  // add mod
      ret = fm.add_mod(target_id, dir);
    } else if (!dir.empty()) {  // add mod
      ret = fm.add_target(dir);
    }
  });

  rmv->callback([&]() {
    std::cout << "rmv callback: target_id=" << target_id << " dir=" << dir
              << " mod_ids.size()=" << mod_ids.size() << "\n";
    auto fm = create_fm();
    if (!mod_ids.empty()) {  // remove mods
      ret = fm.remove_mods(mod_ids);
    } else if (target_id > -1) {  // remove target
      ret = fm.remove_from_target_id(target_id);
    }
  });

  ins->callback([&]() {
    std::cout << "ins callback: target_id=" << target_id << " dir=" << dir
              << " mod_ids.size()=" << mod_ids.size() << "\n";

    auto fm = create_fm();
    if (!mod_ids.empty()) {  // install mods
      ret = fm.install_mods(mod_ids);
    } else if (!dir.empty()) {  // add and install mod directly from mod dir
      ret = fm.install_from_mod_dir(target_id, dir);
    } else if (target_id > -1) {  // install mods from target id
      ret = fm.install_from_target_id(target_id);
    }
  });

  uns->callback([&]() {
    std::cout << "uns callback: target_id=" << target_id << " dir=" << dir
              << " mod_ids.size()=" << mod_ids.size() << "\n";
    auto fm = create_fm();
    if (!mod_ids.empty()) {  // uninstall mods
      ret = fm.uninstall_mods(mod_ids);
    } else if (target_id > -1) {  // uninstall mod from target id
      ret = fm.uninstall_from_target_id(target_id);
    }
  });

  lst->callback([&]() {
    std::cout << "list callback: target_id=" << target_id << " dir=" << dir
              << " mod_ids.size()=" << mod_ids.size()
              << " target_ids.size()=" << target_ids.size() << "\n";

    auto fm = create_fm();
    if (!target_ids.empty()) {  // list targets
      ret.msg = fm.list_targets(target_ids);
    } else if (!mod_ids.empty()) {  // list mods
      ret.msg = fm.list_mods(mod_ids);
    }
  });

  CLI11_PARSE(app, argc, argv)

  if (ret.success) {
    std::cout << ret.msg << std::endl;
    return 0;
  } else {
    std::cerr << ret.msg << "\nfailed\n";
    return 1;
  }
}