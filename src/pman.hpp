#pragma once
#include <csignal>
#include <string>
#include <vector>

#include "util.hpp"
#include "conf_parser.hpp"
#include "pid_file.hpp"
#include "program.hpp"

#define LOG std::cout << "[" << nowString() << "] "

class Pman {
private:
  PmanConf conf;
  PidFile pidFile;
  std::vector<Program> programs;

  void daemonize();
  void registerAbrt();
  void registerSigchld();
  void handleSigchld();
  void startProgram(Program &program);
  void startAllPrograms();
  Program *getProgram(int pid);

public:
  Pman(PmanConf conf)
    : conf(conf), pidFile(PidFile(conf.pidfile)) {}
  Pman(PmanConf conf, std::vector<ProgramConf> programConfs);
  void cleanup();
  int run();
};