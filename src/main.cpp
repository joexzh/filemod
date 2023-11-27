//
// Created by joexie on 11/26/23.
//
#include <iostream>
#include <CLI/CLI.hpp>

int main(int argc, char **argv) {
    CLI::App app{"filemod is a command line file replacement manager."};
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
            int prefix_size = 13;
            std::string msg;
            msg.reserve(str.size() + prefix_size);
            msg += "invalid str: ";
            msg += str;
            return msg;
        }
        return {};
    };

    app.require_subcommand(1);

    auto add = app.add_subcommand("add", "add target or mod to management");
    std::string add_t;
    add->add_option("-t,--target", add_t, "target name")->required()->check(name_validator);
    std::string add_m;
    add->add_option("-m,--mod", add_m, "mod name")->needs("-t")->check(name_validator);
    std::string add_d;
    add->add_option("dir", add_d, "target or mod directory")->needs("-t")->required()->check(CLI::ExistingDirectory);

    auto rmv = app.add_subcommand("remove", "remove profile or mod from management");
    std::string rmv_t;
    rmv->add_option("-t,--target", rmv_t, "target name")->required()->check(name_validator);
    std::string rmv_m;
    rmv->add_option("-m,--mod", rmv_m, "mod name")->needs("-t")->check(name_validator);

    auto ins = app.add_subcommand("install", "install mod to target directory");
    std::string ins_t;
    ins->add_option("-t,--target", ins_t, "target name")->required()->check(name_validator);
    std::string int_m;
    ins->add_option("-m,--mod", int_m, "mod name")->required()->check(name_validator);

    auto uns = app.add_subcommand("uninstall", "uninstall mod from target directory");
    std::string uns_t;
    uns->add_option("-t,--target", uns_t, "target name")->required()->check(name_validator);
    std::string uns_m;
    uns->add_option("-m,--mod", uns_m, "mod name")->required()->check(name_validator);

    auto lst = app.add_subcommand("list", "list managed targets and mods");
    std::string lst_t;
    lst->add_option("-t,--target", lst_t, "target name")->check(name_validator);

    CLI11_PARSE(app, argc, argv)

    return 0;
}
