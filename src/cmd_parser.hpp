#include <string>
#include "defines.h"

enum Command {
  DAEMON,
  STATUS,
  START,
  STOP,
  RESTART,
  KILL,
  ECHO
};

class CmdParser {
private:
  int argc;
  char **argv;
  int cursor;
  Command command_;
  std::string program_;
  std::string conffile_;
  // step cursor if cursor is not the end
  bool consume();
  void back() { cursor--; }
  char *current() { return argv[cursor]; }
  bool hasNext();
  bool match(const char *str);
  bool isCommand();
  void parseCommand();
  void parseArgs();
  bool isConf();
  bool isVersion();
  bool isHelp();
  void showVersion();
  std::string getCommandStr();
  std::string usage();

public:
  CmdParser(int argc, char **argv)
    : argc(argc), argv(argv), cursor(0), command_(DAEMON), program_(""), conffile_(DEFAULT_CONF_PATH) {};
  void parse();
  Command command() { return this->command_; }
  std::string conffile() { return this->conffile_; }
  std::string program() { return this->program_; }
};
