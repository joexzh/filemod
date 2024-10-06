//
// Created by Joe Tse on 11/26/23.
//
#include <CLI/CLI.hpp>
#include <cstdlib>
#include <exception>
#include <filemod/modder.hpp>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

static inline bool is_set(int64_t id) {
  return id != std::numeric_limits<int64_t>::min();
}

static inline bool is_set(const std::vector<int64_t> &ids) {
  return !ids.empty();
}

static inline bool is_set(const std::string &dir) { return !dir.empty(); }

static inline void move_to_retbase(filemod::result<int64_t> &&from,
                                   filemod::result_base &to) {
  to.success = from.success;
  if (from.success) {
    to.msg = std::to_string(from.data);
  } else {
    to.msg = std::move(from.msg);
  }
}

static inline int run(int argc, char **argv) {
  CLI::App app{"filemod is a file replacement manager."};
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

  int64_t tar_id{std::numeric_limits<int64_t>::min()};
  std::vector<int64_t> tar_ids;
  std::vector<int64_t> mod_ids;
  std::string dir;

  auto add = app.add_subcommand("add", "add target or mod to management");
  add->add_option("--tdir", dir, "target directory")
      ->take_last()
      ->check(CLI::ExistingDirectory);
  auto mdir_opt =
      add->add_option("--mdir", dir, "mod directory")->excludes("--tdir");
  add->add_option("-t", tar_id, "target id")
      ->excludes("--tdir")
      ->needs("--mdir")
      ->take_last()
      ->check(name_validator);
  mdir_opt->needs("-t")->take_last()->check(CLI::ExistingDirectory);

  auto ins = app.add_subcommand("install", "install mod to target directory");
  ins->add_option("-t", tar_id, "target id")
      ->take_last()
      ->check(name_validator);
  ins->add_option("--mdir", dir, "mod directory")
      ->needs("-t")
      ->take_last()
      ->check(CLI::ExistingDirectory);
  ins->add_option("-m", mod_ids, "mod ids")
      ->excludes("-t", "--mdir")
      ->take_all()
      ->check(name_validator);

  auto uns =
      app.add_subcommand("uninstall", "uninstall mod from target directory");
  uns->add_option("-t", tar_id, "target id")
      ->take_last()
      ->check(name_validator);
  uns->add_option("-m", mod_ids, "mod ids")
      ->excludes("-t")
      ->take_all()
      ->check(name_validator);

  auto rmv =
      app.add_subcommand("remove", "remove target or mod from management");
  rmv->add_option("-t", tar_id, "target id")
      ->take_last()
      ->check(name_validator);
  rmv->add_option("-m", mod_ids, "mod ids")
      ->excludes("-t")
      ->take_all()
      ->check(name_validator)
      ->take_last();

  auto lst = app.add_subcommand("list", "list managed targets and mods");
  lst->add_option("-t", tar_ids, "target ids")
      ->take_all()
      ->check(name_validator);
  lst->add_option("-m", mod_ids, "mod ids")
      ->excludes("-t")
      ->take_all()
      ->check(name_validator);

  filemod::result_base ret{.success = true};

  add->callback([&]() {
    auto modder = filemod::create_modder();
    if (is_set(tar_id) && is_set(dir)) {  // add mod
      move_to_retbase(modder.add_mod(tar_id, dir), ret);
    } else if (is_set(dir)) {  // add target
      move_to_retbase(modder.add_target(dir), ret);
    }
  });

  rmv->callback([&]() {
    auto modder = filemod::create_modder();
    if (is_set(mod_ids)) {  // remove mods
      ret = modder.remove_mods(mod_ids);
    } else if (is_set(tar_id)) {  // remove target
      ret = modder.remove_from_target_id(tar_id);
    }
  });

  ins->callback([&]() {
    auto modder = filemod::create_modder();
    if (is_set(mod_ids)) {  // install mods
      ret = modder.install_mods(mod_ids);
    } else if (is_set(tar_id) &&
               is_set(dir)) {  // add and install mod directly from mod dir
      move_to_retbase(modder.install_from_mod_src(tar_id, dir), ret);
    } else if (is_set(tar_id)) {  // install mods from target id
      ret = modder.install_from_target_id(tar_id);
    }
  });

  uns->callback([&]() {
    auto modder = filemod::create_modder();
    if (is_set(mod_ids)) {  // uninstall mods
      ret = modder.uninstall_mods(mod_ids);
    } else if (is_set(tar_id)) {  // uninstall mod from target id
      ret = modder.uninstall_from_target_id(tar_id);
    }
  });

  lst->callback([&]() {
    auto modder = filemod::create_modder();
    if (is_set(mod_ids)) {  // list mods
      ret.msg = modder.list_mods(mod_ids);
    } else {  // list targets
      ret.msg = modder.list_targets(tar_ids);
    }
  });

  CLI11_PARSE(app, argc, argv)

  if (!ret.success) {
    std::cerr << ret.msg << '\n';
    return EXIT_FAILURE;
  }
  std::cout << ret.msg << '\n';
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  try {
    return run(argc, argv);
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }
}