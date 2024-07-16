//
// Created by Joe Tse on 11/26/23.
//
#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "CLI/Validators.hpp"
#include "filemod.h"

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

  std::string target_id;
  std::vector<std::string> target_ids;
  std::vector<std::string> mod_ids;
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
  lst->add_flag("--v", "verbose")->excludes("--t")->needs("--m");

  filemod::result<std::string> ret;

  add->callback([&]() {
    std::cout << "add callback: target id: " << target_id << " dir: " << dir
              << "\n";
  });

  rmv->callback([&]() {

  });

  ins->callback([&]() {

  });

  uns->callback([&]() {

  });

  lst->callback([&]() {

  });

  CLI11_PARSE(app, argc, argv)

  if (ret.success) {
    std::puts("succeed");
    return 0;
  } else {
    std::cerr << ret.data << "\nfailed\n";
    return 1;
  }
}