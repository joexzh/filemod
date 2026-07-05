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
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

namespace po = boost::program_options;

static bool is_set(int64_t id) {
  return id != (std::numeric_limits<int64_t>::min)();
}

static bool is_set(const std::vector<int64_t>& ids) { return !ids.empty(); }

static bool is_set(const std::string& dir) { return !dir.empty(); }

static void append_ok(filemod::result_base& ret) {
  if (ret.success) {
    if (!ret.msg.empty()) {
      ret.msg += '\n';
    }
    ret.msg += "ok";
  }
}

static void move_to_retbase(filemod::result_base&& from,
                            filemod::result_base& to) {
  to = std::move(from);
  append_ok(to);
}

static void move_to_retbase(filemod::result<int64_t>&& from,
                            filemod::result_base& to) {
  to.success = from.success;
  if (from.success) {
    to.msg = std::to_string(from.data) + '\n' + "ok";
  } else {
    to.msg = std::move(from.msg);
  }
}

constexpr char FILEMOD_MARGIN[] = "    ";

static void mod_files_to_string(const std::vector<std::string>& file_strs,
                                std::string_view& margin, std::string& ret) {
  for (auto& file_str : file_strs) {
    ret += margin;
    ret += '\'';
    ret += file_str;
    ret += '\'';
    ret += '\n';
  }
}

// Return format:
//
// MOD_ID 222 DIR e/f/g STATUS installed
//     MOD_FILES
//         'a/b/c'
//         'e/f/g'
//         'r/g/c'
//         'a'
//     BACKUP_FILES
//         'xxx'
// MOD_ID 333 DIR 'x/y/z' STATUS not_installed
//     MOD_FILES
//         ...
//     BACKUP_FILES
//         ...
static std::string mods_to_string(const std::vector<filemod::ModDto>& mods,
                                  bool verbose = false, uint8_t indent = 0) {
  std::string ret;

  std::string full_margin;
  for (int i = 0; i < indent + 2; ++i) {
    full_margin += FILEMOD_MARGIN;
  }
  std::string_view margin1{full_margin.c_str(),
                           indent * filemod::length_s(FILEMOD_MARGIN)};
  std::string_view margin2{full_margin.c_str(),
                           (indent + 1) * filemod::length_s(FILEMOD_MARGIN)};
  std::string_view margin3{full_margin.c_str(),
                           (indent + 2) * filemod::length_s(FILEMOD_MARGIN)};

  for (auto& mod : mods) {
    ret += margin1;
    ret += "MOD_ID ";
    ret += std::to_string(mod.id);
    ret += " DIR '";
    ret += mod.dir;
    ret += "' STATUS ";
    ret += mod.status == filemod::ModStatus::Installed ? "installed"
                                                       : "not_installed";
    ret += '\n';
    if (verbose) {
      ret += margin2;
      ret += "MOD_FILES\n";
      mod_files_to_string(mod.files, margin3, ret);
      ret += margin2;
      ret += "BACKUP_FILES\n";
      mod_files_to_string(mod.bak_files, margin3, ret);
    }
  }

  return ret;
}

// Return format:
//
// TARGET_ID 111 DIR '/a/b/c'
//     MOD_ID 222 DIR 'e/f/g' STATUS installed
//     MOD_ID 333 DIR 'x/y/z' STATUS not_installed
static std::string targets_to_string(
    const std::vector<filemod::TargetDto>& tars) {
  std::string ret;

  for (auto& tar : tars) {
    ret += "TARGET_ID ";
    ret += std::to_string(tar.id);
    ret += " DIR '";
    ret += tar.dir;
    ret += "'\n";

    ret += mods_to_string(tar.ModDtos, false, 1);
  }

  return ret;
}

static void parse_subcmd(const po::options_description& desc,
                         const po::basic_parsed_options<char>& parsed,
                         po::variables_map& vm) {
  auto opts = po::collect_unrecognized(parsed.options, po::include_positional);
  opts.erase(opts.begin());

  // parse again
  po::store(po::command_line_parser(opts).options(desc).run(), vm);
  po::notify(vm);
}

static void parse_error(const po::options_description& desc,
                        std::ostream& ostream, filemod::result_base& ret) {
  ostream << desc;
  ret.success = false;
}

static void parse_add(filemod::result_base& ret, std::ostringstream& oss,
                      po::basic_parsed_options<char>& parsed,
                      po::variables_map& vm, int64_t& id, std::string& name,
                      std::string& dir) {
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
    move_to_retbase(md.add_target(dir), ret);
  } else if (vm.count("tid") &&
             vm.count("mdir")) {  // add mod from mod source directory
    if (vm.count("name")) {
      move_to_retbase(md.add_mod(id, name, dir), ret);
    } else {
      move_to_retbase(md.add_mod(id, dir), ret);
    }
  } else if (vm.count("tid") && vm.count("archive")) {  // add mod from archive
    if (vm.count("name")) {
      move_to_retbase(md.add_mod_archive(id, name, dir), ret);
    } else {
      move_to_retbase(md.add_mod_archive(id, dir), ret);
    }
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_install(filemod::result_base& ret, std::ostringstream& oss,
                          po::basic_parsed_options<char>& parsed,
                          po::variables_map& vm, int64_t& id, std::string& name,
                          std::string& dir, std::vector<int64_t>& ids) {
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
        move_to_retbase(md.install_mod_path(id, name, dir), ret);
      } else {
        move_to_retbase(md.install_mod_path(id, dir), ret);
      }
    } else if (vm.count("archive")) {
      if (vm.count("name")) {
        move_to_retbase(md.install_mod_archive(id, name, dir), ret);
      } else {
        move_to_retbase(md.install_mod_archive(id, dir), ret);
      }
    } else {
      ret = md.install_target(id);
    }
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_uninstall(filemod::result_base& ret, std::ostringstream& oss,
                            po::basic_parsed_options<char>& parsed,
                            po::variables_map& vm, int64_t& id,
                            std::vector<int64_t>& ids) {
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
    move_to_retbase(md.uninstall_target(id), ret);
  } else if (is_set(ids)) {  // uninstall multiple mods
    move_to_retbase(md.uninstall_mods(ids), ret);
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_remove(filemod::result_base& ret, std::ostringstream& oss,
                         po::basic_parsed_options<char>& parsed,
                         po::variables_map& vm, int64_t& id,
                         std::vector<int64_t>& ids) {
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
    move_to_retbase(md.remove_target(id), ret);
  } else if (is_set(ids)) {  // remove multiple mods
    move_to_retbase(md.remove_mods(ids), ret);
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_list(filemod::result_base& ret, std::ostringstream& oss,
                       po::basic_parsed_options<char>& parsed,
                       po::variables_map& vm, std::vector<int64_t>& ids) {
  po::options_description desc(
      "display target(s) and mod(s) in database\n"
      "Usage: filemod list [-t <target_id1> [target_id2] ...]\n"
      "       filemod list -m <mod_id1> [mod_id2] ...\n"
      "Options");
  desc.add_options()(
      "tid,t", po::value<std::vector<int64_t>>(&ids)->multitoken(),
      "target ids")("mid,m", po::value<std::vector<int64_t>>()->multitoken(),
                    "mod ids")("help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (vm.count("mid")) {  // list mods
    ret.msg =
        mods_to_string(md.query_mods(vm["mid"].as<std::vector<int64_t>>()));
  } else {  // list targets
    ret.msg = targets_to_string(md.query_targets(ids));
  }
}

static void parse_rename(filemod::result_base& ret, std::ostringstream& oss,
                         po::parsed_options& parsed, po::variables_map& vm,
                         int64_t& mid, std::string& newname) {
  po::options_description desc(
      "rename mod\n"
      "Usage: filemod rename -m <mod_id> -n <newname>\n"
      "Options");
  desc.add_options()("mid,m", po::value<int64_t>(&mid), "mod id")(
      "name,n", po::value<std::string>(&newname), "new mod name")("help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (vm.count("mid") && vm.count("name")) {
    move_to_retbase(md.rename_mod(mid, newname), ret);
  } else {
    parse_error(desc, oss, ret);
  }
}

static int parse(int argc, char* argv[]) {
  filemod::result_base ret{.success = true};
  std::ostringstream oss;

  po::options_description visible(
      "filemod is a file replacement manager.\n"
      "Usage: filemod <command> <args>\n"
      " Commands: add | install | uninstall | remove | list | rename\n"
      " filemod <command> --help to show command help.\n"
      "Common Options");
  visible.add_options()("help,h", "")("version,v", "");

  po::options_description hidden("command");
  hidden.add_options()("command", po::value<std::string>(), "")(
      "subargs", po::value<std::vector<std::string>>(), "");

  po::options_description all;
  all.add(visible).add(hidden);

  po::positional_options_description subcmd;
  subcmd.add("command", 1).add("subargs", -1);

  auto parsed = po::command_line_parser(argc, argv)
                    .options(all)
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
    } else if ("rename" == cmd) {
      parse_rename(ret, oss, parsed, vm, id, name);
    } else {
      parse_error(visible, oss, ret);
    }
  } else if (vm.count("help")) {
    oss << visible;
  } else if (vm.count("version")) {
    ret.msg = STRINGIFY(FILEMOD_VERSION);
  } else {
    parse_error(visible, oss, ret);
  }

  if (ret.success) {
    std::cout << ret.msg << oss.str() << '\n';
    return 0;
  }
  std::cerr << ret.msg << oss.str() << '\n';
  return 1;
}

int main(int argc, char** argv) {
#ifdef _WIN32
  // Must inject UTF-8 manifest on Windows. See
  // https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page

  // For legacy CMD which does not support auto converting input to UTF-8.

  setlocale(LC_ALL, ".UTF-8");
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  LPWSTR* szArgList = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (szArgList == nullptr) {
    std::cerr << "Error: fail to parse arguments\n";
    return 1;
  }

  struct scope_guard {
    LPWSTR* ptr;
    explicit scope_guard(LPWSTR* p) : ptr{p} {}
    ~scope_guard() { LocalFree(ptr); }
  } scope_guard{szArgList};

  constexpr std::size_t MAX_ARGC = 64;
  if (argc > MAX_ARGC) {
    std::cerr << "Error: too many arguments, max is " << MAX_ARGC << '\n';
    return 1;
  }
  char* arr_pchar[MAX_ARGC];

  std::vector<std::string> utf8_argv;
  utf8_argv.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    utf8_argv.push_back(filemod::wstr_to_cp(szArgList[i], CP_UTF8));
    arr_pchar[i] = utf8_argv[i].data();
  }
  argv = arr_pchar;
#else
  setlocale(LC_ALL, "");  // trust the env on *nix system.
#endif

  try {
    // assume argv are all utf-8 encoded
    return parse(argc, argv);
  } catch (std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
