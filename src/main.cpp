//
// Created by Joe Tse on 11/26/23.
//
#include <iostream>
#include <cstdio>
#include <string>
#include <CLI/CLI.hpp>

#include "filemod.h"
#include "utils.h"

int main(int argc, char **argv) {
    if (!real_effective_user_match()) {
        std::cerr << "Please don't use sudo to run this program!\n" << std::endl;
        return 1;
    }

    CLI::App app{"filemod is file replacement manager."};
    app.set_help_all_flag("--help-all");

    class MyFormatter : public CLI::Formatter {
    public:
        MyFormatter() : Formatter() {}

        std::string make_option_opts(const CLI::Option *) const override { return ""; }
    };

    auto fmt = std::make_shared<MyFormatter>();
    app.formatter(fmt);

    auto name_validator = [](const std::string &str) -> std::string {
        if (str.empty() || str[0] == '-' || std::string_view(str).substr(0, 2) == "--") {
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

    std::string target_name;
    std::string mod_name;
    std::string dir;

    auto add = app.add_subcommand("add", "add target or mod to management");
    add->add_option("-t,--target", target_name, "target name")->required()->check(name_validator)->take_last();
    add->add_option("-m,--mod", mod_name, "mod name")->needs("-t")->check(name_validator)->take_last();
    add->add_option("dir", dir, "target or mod directory")->needs("-t")->required()->check(CLI::ExistingDirectory);

    auto rmv = app.add_subcommand("remove", "remove profile or mod from management");
    rmv->add_option("-t,--target", target_name, "target name")->required()->check(name_validator)->take_last();
    rmv->add_option("-m,--mod", mod_name, "mod name")->needs("-t")->check(name_validator)->take_last();

    auto ins = app.add_subcommand("install", "install mod to target directory");
    ins->add_option("-t,--target", target_name, "target name")->required()->check(name_validator)->take_last();
    ins->add_option("-m,--mod", mod_name, "mod name")->required()->check(name_validator)->take_last();

    auto uns = app.add_subcommand("uninstall", "uninstall mod from target directory");
    uns->add_option("-t,--target", target_name, "target name")->required()->check(name_validator)->take_last();
    uns->add_option("-m,--mod", mod_name, "mod name")->required()->check(name_validator)->take_last();

    auto lst = app.add_subcommand("list", "list managed targets and mods");
    lst->add_option("-t,--target", target_name, "target name")->check(name_validator)->take_last();

    filemod::result ret;

    add->callback([&]() {
        std::cout << "add callback: target: " << target_name << " mod: " << mod_name << " dir: " << dir << "\n";

        if (mod_name.empty()) {
            ret = filemod::add_target(target_name, dir);
        } else {
            ret = filemod::add_mod(target_name, mod_name, dir);
        }
    });

    rmv->callback([&]() {
        if (mod_name.empty()) {
            ret = filemod::remove_target(target_name);
        } else {
            ret = filemod::remove_mod(target_name, mod_name);
        }
    });

    ins->callback([&]() {
        ret = filemod::install_mod(target_name, mod_name);
    });

    uns->callback([&]() {
        ret = filemod::uninstall_mod(target_name, mod_name);
    });

    lst->callback([&]() {
        ret = filemod::list_all(target_name);
    });

    CLI11_PARSE(app, argc, argv)

    if (ret.success) {
        std::puts("succeed");
        return 0;
    } else {
        std::cerr << ret.msg << "\nfailed\n";
        return 1;
    }
}
