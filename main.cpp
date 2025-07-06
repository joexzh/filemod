//
// Created by Joe Tse on 11/26/23.
//
#include <boost/program_options.hpp>
#include <exception>
#include <filemod/modder.hpp>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace po = boost::program_options;

static bool is_set(int64_t id) {
  return id != std::numeric_limits<int64_t>::min();
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
                      po::variables_map &vm, std::string &dir, int64_t &id) {
  po::options_description desc(
      "add target or mod\n"
      "Usage: filemod add --tdir <target_dir>\n"
      "       filemod add -t <target_id> --mdir <mod_dir>\n"
      "Options");
  desc.add_options()("tdir", po::value<std::string>(&dir), "target directory")(
      "tid,t", po::value<int64_t>(&id), "target id")(
      "mdir", po::value<std::string>(&dir), "mod source files directory")(
      "help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (is_set(dir) && is_set(id)) {  // add mod to existing target
    move_to_retbase(md.add_mod(id, dir), ret);
  } else if (is_set(dir)) {  // add target
    move_to_retbase(md.add_target(dir), ret);
  } else {
    parse_error(desc, oss, ret);
  }
}

static void parse_install(filemod::result_base &ret, std::ostringstream &oss,
                          po::basic_parsed_options<char> &parsed,
                          po::variables_map &vm, std::string &dir, int64_t &id,
                          std::vector<int64_t> &ids) {
  po::options_description desc(
      "install mod(s)\n"
      "Usage: filemod install -t <target_id>\n"
      "       filemod install -t <target_id> --mdir <mod_dir>\n"
      "       filemod install -m <mod_id1>...\n"
      "Options");
  desc.add_options()("tid,t", po::value<int64_t>(&id), "target id")(
      "mdir", po::value<std::string>(&dir), "mod source directory")(
      "mid,m", po::value<std::vector<int64_t>>(&ids)->multitoken(), "mod ids")(
      "help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (is_set(dir) && is_set(id)) {  // install from mod source dir
    move_to_retbase(md.install_from_mod_dir(id, dir), ret);
  } else if (is_set(id)) {  // install all mods of a target
    ret = md.install_from_target_id(id);
  } else if (is_set(ids)) {  // install multiple mods
    ret = md.install_mods(ids);
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
      "       filemod uninstall -m <mod_id1>...\n"
      "Options");
  desc.add_options()("tid,t", po::value<int64_t>(&id), "target id")(
      "mid,m", po::value<std::vector<int64_t>>(&ids)->multitoken(), "mod ids")(
      "help,h", "");
  parse_subcmd(desc, parsed, vm);
  filemod::modder md;

  if (vm.count("help")) {
    oss << desc;
  } else if (is_set(id)) {  // uninstall all mods of a target
    ret = md.uninstall_from_target_id(id);
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
      "       filemod remove -m <mod_id1>...\n"
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
      "Usage: filemod list [-t <target_id1>...]\n"
      "       filemod list -m <mod_id1>...\n"
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

int parse(int argc, char *argv[]) {
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

    int64_t id = std::numeric_limits<int64_t>::min();
    std::string dir;
    std::vector<int64_t> ids;

    if ("add" == cmd) {
      parse_add(ret, oss, parsed, vm, dir, id);
    } else if ("install" == cmd) {
      parse_install(ret, oss, parsed, vm, dir, id, ids);
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
    std::cout << ret.msg << oss.str() << '\n';
    return 0;
  }
  std::cerr << ret.msg << oss.str() << '\n';
  return 1;
}

int main(int argc, char **argv) {
  try {
    return parse(argc, argv);
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}