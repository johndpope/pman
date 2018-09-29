#include <string>
#include <vector>
#include <time.h>
#include "conf_parser.hpp"

#define RESTART_MIN_SEC 1

class Program {
private:
  ProgramConf conf;
  bool isRunning_;
  int pid_;
  int execCount_;
  time_t startTime_;

public:
  Program(ProgramConf conf)
    : conf(conf), isRunning_(false), pid_(0), execCount_(0), startTime_(0) {}
  std::string logfile() { return this->conf.logfile; }
  std::string name() { return this->conf.name; }
  std::vector<std::string> command() { return this->conf.command; }
  bool isRunning() { return this->isRunning_; }
  int pid() { return this->pid_; }
  int execCount() { return this->execCount_; }
  bool autorestart() { return this->conf.autorestart; }
  bool tooShort();
  void started(int pid);
  void stopped();
};