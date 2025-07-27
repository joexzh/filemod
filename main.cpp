//
// Created by Joe Tse on 11/26/23.
//
#include <boost/program_options.hpp>
#include <exception>
#include <filemod/modder.hpp>
#include <filemod/utils.hpp>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>

#define MAIN wmain
typedef wchar_t mychar;

static void put_exception(std::exception &e) {
  std::wcerr << filemod::cp_to_wstr(e.what(), CP_UTF8) << L'\n';
}

static void put_os(std::wostream &wos, const std::string &msg,
                   std::ostringstream &oss) {
  wos << filemod::cp_to_wstr(msg, CP_UTF8)
      << filemod::cp_to_wstr(oss.str(), CP_UTF8) << L'\n';
}

static void put_msg(const std::string &msg, std::ostringstream &oss) {
  put_os(std::wcout, msg, oss);
}

static void put_err(const std::string &msg, std::ostringstream &oss) {
  put_os(std::wcerr, msg, oss);
}

#else
#define MAIN main
typedef char mychar;

static void put_exception(std::exception &e) { std::cerr << e.what() << '\n'; }

static void put_os(std::ostream &os, const std::string &msg,
                   std::ostringstream &oss) {
  os << msg << oss.str() << '\n';
}

static void put_msg(const std::string &msg, std::ostringstream &oss) {
  put_os(std::cout, msg, oss);
}

static void put_err(const std::string &msg, std::ostringstream &oss) {
  put_os(std::cerr, msg, oss);
}

#endif

namespace po = boost::program_options;

static bool is_set(int64_t id) {
  return id != (std::numeric_limits<int64_t>::min)();
}

static bool is_set(const std::vector<int64_t> &ids) { return !ids.empty(); }

static bool is_set(const std::string &dir) { return !dir.empty(); }

static void move_to_retbase(filemod::result<int64_t> &&from,
                            filemod::result_base &to) {
  to.success = from.success;
  if (from.success) {
    to.msg = std::to_string(from.data);
  } else {
    to.msg = std::move(from.msg);
  }
}

static void parse_subcmd(const po::options_description &desc,
                         const po::basic_parsed_options<char> &parsed,
                         po::variables_map &vm) {
  auto opts = po::collect_unrecognized(parsed.options, po::include_positional);
  opts.erase(opts.begin());

  // parse again
  po::store(po::command_line_parser(opts).options(desc).run(), vm);
  po::notify(vm);
}

static void parse_error(const po::options_description &desc,
                        std::ostream &ostream, filemod::result_base &ret) {
  ostream << desc;
  ret.success = false;
}

static void parse_add(filemod::result_base &ret, std::ostringstream &oss,
                      po::basic_parsed_options<char> &parsed,
                      po::variables_map &vm, int64_t &id, std::string &name,
                      std::string &dir) {
  po::options_description desc(
      "add target or mod\n"
      "Usage: filemod add --tdir <target_dir>\n"
      "       filemod add -t <target_id> [--name <mod_name>] --mdir "
      "<mod_dir>\n"
      "       filemod add -t <target_id> [--name <mod_name>] --archive "
      "<archive_path>\n"
      "Options");
  desc.add_options()("tdir", po::value<std::string>(&dir), "target directory")(
      "tid,t", po::value<int64_t>(&id), "target id")(
      "name,n", po::value<std::string>(&name), "mod name")(
      "mdir,d", po::value<std::string>(&dir), "mod source files directory")(
      "archive,a", po::value<std::string>(&dir), "mod archie path")("help,h",
                                                                    "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (vm.count("tdir")) {  // add target
    move_to_retbase(md.add_target(filemod::utf8str_to_path(dir)), ret);
  } else if (vm.count("tid") &&
             vm.count("mdir")) {  // add mod from mod source directory
    if (vm.count("name")) {
      move_to_retbase(md.add_mod(id, name, filemod::utf8str_to_path(dir)), ret);
    } else {
      move_to_retbase(md.add_mod(id, dir), ret);
    }
  } else if (vm.count("tid") && vm.count("archive")) {  // add mod from archive
    if (vm.count("name")) {
      move_to_retbase(md.add_mod_a(id, name, filemod::utf8str_to_path(dir)),
                      ret);
    } else {
      move_to_retbase(md.add_mod_a(id, filemod::utf8str_to_path(dir)), ret);
    }
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_install(filemod::result_base &ret, std::ostringstream &oss,
                          po::basic_parsed_options<char> &parsed,
                          po::variables_map &vm, int64_t &id, std::string &name,
                          std::string &dir, std::vector<int64_t> &ids) {
  po::options_description desc(
      "install mod(s)\n"
      "Usage: filemod install -t <target_id>\n"
      "       filemod install -m <mod_id1> [mod_id2] ...\n"
      "       filemod install -t <target_id> [--name <mod_name>] --mdir "
      "<mod_dir>\n"
      "       filemod install -t <target_id> [--name <mod_name>] -a <archive>\n"
      "Options");
  desc.add_options()("tid,t", po::value<int64_t>(&id), "target id")(
      "name,n", po::value<std::string>(&name), "mod name")(
      "mdir,d", po::value<std::string>(&dir), "mod source directory")(
      "archive,a", po::value<std::string>(&dir), "mod archie path")(
      "mid,m", po::value<std::vector<int64_t>>(&ids)->multitoken(), "mod ids")(
      "help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (vm.count("mid")) {
    ret = md.install_mods(ids);
  } else if (vm.count("tid")) {
    if (vm.count("mdir")) {
      if (vm.count("name")) {
        move_to_retbase(
            md.install_path(id, name, filemod::utf8str_to_path(dir)), ret);
      } else {
        move_to_retbase(md.install_path(id, filemod::utf8str_to_path(dir)),
                        ret);
      }
    } else if (vm.count("archive")) {
      if (vm.count("name")) {
        move_to_retbase(
            md.install_path_a(id, name, filemod::utf8str_to_path(dir)), ret);
      } else {
        move_to_retbase(md.install_path_a(id, filemod::utf8str_to_path(dir)),
                        ret);
      }
    } else {
      ret = md.install_target(id);
    }
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_uninstall(filemod::result_base &ret, std::ostringstream &oss,
                            po::basic_parsed_options<char> &parsed,
                            po::variables_map &vm, int64_t &id,
                            std::vector<int64_t> &ids) {
  po::options_description desc(
      "uninstall mod(s)\n"
      "Usage: filemod uninstall -t <target_id>\n"
      "       filemod uninstall -m <mod_id1> [mod_id2] ...\n"
      "Options");
  desc.add_options()("tid,t", po::value<int64_t>(&id), "target id")(
      "mid,m", po::value<std::vector<int64_t>>(&ids)->multitoken(), "mod ids")(
      "help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (is_set(id)) {  // uninstall all mods of a target
    ret = md.uninstall_target(id);
  } else if (is_set(ids)) {  // uninstall multiple mods
    ret = md.uninstall_mods(ids);
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_remove(filemod::result_base &ret, std::ostringstream &oss,
                         po::basic_parsed_options<char> &parsed,
                         po::variables_map &vm, int64_t &id,
                         std::vector<int64_t> &ids) {
  po::options_description desc(
      "remove target or mod(s)\n"
      "Usage: filemod remove -t <target_id>\n"
      "       filemod remove -m <mod_id1> [mod_id2] ...\n"
      "Options");
  desc.add_options()("tid,t", po::value<int64_t>(&id), "target id")(
      "mid,m", po::value<std::vector<int64_t>>(&ids)->multitoken(), "mod ids")(
      "help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (is_set(id)) {  // remove mod from a target
    ret = md.remove_target(id);
  } else if (is_set(ids)) {  // remove multiple mods
    ret = md.remove_mods(ids);
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_list(filemod::result_base &ret, std::ostringstream &oss,
                       po::basic_parsed_options<char> &parsed,
                       po::variables_map &vm, std::vector<int64_t> &ids) {
  po::options_description list_desc(
      "display target(s) and mod(s) in database\n"
      "Usage: filemod list [-t <target_id1> [target_id2] ...]\n"
      "       filemod list -m <mod_id1> [mod_id2] ...\n"
      "Options");
  list_desc.add_options()(
      "tid,t", po::value<std::vector<int64_t>>(&ids)->multitoken(),
      "target ids")("mid,m", po::value<std::vector<int64_t>>()->multitoken(),
                    "mod ids")("help,h", "");
  parse_subcmd(list_desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << list_desc;
  } else if (vm.count("mid")) {  // list mods
    ret.msg = md.list_mods(vm["mid"].as<std::vector<int64_t>>());
  } else {  // list targets
    ret.msg = md.list_targets(ids);
  }
}

int parse(int argc, const char *argv[]) {
  filemod::result_base ret{.success = true};
  std::ostringstream oss;

  po::options_description global_desc(
      "filemod is a file replacement manager.\n"
      "Usage: filemod <command> [<args>]\n"
      "Commands: add | install | uninstall | remove | list\n"
      "Global Options");
  global_desc.add_options()("help,h", "")("version,v", "");

  po::options_description hidden("command");
  hidden.add_options()("command", po::value<std::string>(), "")(
      "subargs", po::value<std::vector<std::string>>(), "");

  po::options_description global_cmd_desc;
  global_cmd_desc.add(global_desc).add(hidden);

  po::positional_options_description subcmd;
  subcmd.add("command", 1).add("subargs", -1);

  auto parsed = po::command_line_parser(argc, argv)
                    .options(global_cmd_desc)
                    .positional(subcmd)
                    .allow_unregistered()
                    .run();
  po::variables_map vm;
  po::store(parsed, vm);
  po::notify(vm);

  if (vm.count("command")) {
    auto cmd = vm["command"].as<std::string>();

    int64_t id = (std::numeric_limits<int64_t>::min)();
    std::string name;
    std::string dir;
    std::vector<int64_t> ids;

    if ("add" == cmd) {
      parse_add(ret, oss, parsed, vm, id, name, dir);
    } else if ("install" == cmd) {
      parse_install(ret, oss, parsed, vm, id, name, dir, ids);
    } else if ("uninstall" == cmd) {
      parse_uninstall(ret, oss, parsed, vm, id, ids);
    } else if ("remove" == cmd) {
      parse_remove(ret, oss, parsed, vm, id, ids);
    } else if ("list" == cmd) {
      parse_list(ret, oss, parsed, vm, ids);
    } else {
      parse_error(global_desc, oss, ret);
    }
  } else if (vm.count("help")) {
    oss << global_desc;
  } else if (vm.count("version")) {
    ret.msg = STRINGIFY(FILEMOD_VERSION);
  } else {
    parse_error(global_desc, oss, ret);
  }

  if (ret.success) {
    put_msg(ret.msg, oss);
    return 0;
  }
  put_err(ret.msg, oss);
  return 1;
}

int MAIN(int argc, mychar **argv) {
  setlocale(LC_CTYPE, "en_US.UTF-8");

#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  // convert wchar** to utf8 char** on Windows
  std::unique_ptr<const char *[], void (*)(const char **p)> ptr {
    new const char *[argc], [](const char **p) { delete[] p; }
  };
  std::vector<std::string> str;
  str.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    str.push_back(filemod::wstr_to_cp(argv[i], CP_UTF8));
    ptr[i] = str[i].c_str();
  }
  const char **args = ptr.get();
#else
  char **args = argv;
#endif

  try {
    // args are all utf-8 encoded
    return parse(argc, const_cast<const char **>(args));
  } catch (std::exception &e) {
    put_exception(e);
    return 1;
  }
}