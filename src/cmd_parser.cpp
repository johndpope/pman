#include <string>
#include <iostream>
#include <sstream>
#include "defines.h"
#include "cmd_parser.hpp"

using namespace std;

void CmdParser::parse()
{
  if (!consume()) return;

  if (isVersion()) {
    showVersion();
    exit(0);
  }

  if (isHelp()) {
    cout << usage() << endl;
    exit(0);
  }

  if (isCommand()) {
    parseCommand();
    if (!consume()) return;
  }

  parseArgs();
}

bool CmdParser::consume()
{
  if (hasNext()) {
   cursor++;
   return true;
  }
  return false;
};

bool CmdParser::hasNext()
{
  return cursor < (argc - 1);
}

bool CmdParser::match(const char *str)
{
  return strcmp(current(), str) == 0;
}

bool CmdParser::isCommand()
{
  return match("status") || match("start") || match("stop");
}

void CmdParser::parseCommand()
{
  assert(isCommand());

  if (match("status")) {
    this->command_ = E_STATUS;
  } else if (match("start")) {
    this->command_ = E_START;
  } else if (match("stop")) {
    this->command_ = E_STOP;
  }

  // parse program name

  // `start` and `stop` command requires program name.
  // `status` command allows empty, then called `status all` by default.
  if (!hasNext()) {
    switch (command()) {
      case E_START:
      case E_STOP:
        cerr << "requires program name" << endl << endl;
        cerr << usage() << endl;
        exit(1);
      default:
        return;
    }
  }

  consume();

  if (isArgs()) {
    if (command() == E_START || command() == E_STOP) {
      cerr << "requires program name" << endl;
      exit(1);
    } else {
      back();
      return;
    }
  }

  this->program_ = string(current());
}

bool CmdParser::isArgs()
{
  return isConf();
}

bool CmdParser::isConf()
{
  return match("--conf") || match("-c");
}

void CmdParser::parseArgs()
{
  for (;;) {
    if (isConf()) {
      if (!consume()) {
        cerr << "please pass conf file path after `--conf` or `-c`" << endl;
        exit(1);
      }

      this->conffile_ = string(current());
    } else {
      cerr << "unknown option: " << current() << endl << endl;
      cerr << usage() << endl;
      exit(1);
    }

    if (!consume()) return;
  }
}

bool CmdParser::isVersion()
{
  return match("--version") || match("-v");
}

bool CmdParser::isHelp()
{
  return match("--help") || match("-h");
}

void CmdParser::showVersion()
{
  cout << "pman version " << PMAN_VERSION << endl;
}

std::string CmdParser::usage()
{
  std::ostringstream os;
  os << "usage: pman [--version | -v] [--help | -h]" << endl
    << "\t    <command> [<args>]" << endl
    << "command:" << endl
    << "  status <program>\tshow status" << endl
    << "  status all      \tshow status of all programs" << endl
    << "  start <program> \tstart program" << endl
    << "  start all       \tstart all programs that not running" << endl
    << "  stop <program>  \tstop program" << endl
    << "  stop all        \tstop all programs that running" << endl
    << endl
    << "args:" << endl
    << "  --conf, -c <path>\tconfig file path" << endl;
  return os.str();
}
