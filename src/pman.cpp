#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "defines.h"
#include "pman.hpp"

using namespace std;

namespace {
  volatile std::sig_atomic_t abrt_status;
  volatile std::sig_atomic_t sigchld_status;
}
void abrt_handler(int signal) { abrt_status = signal; };
void sigchld_handler(int signal) { sigchld_status = signal; };

Pman::Pman(PmanConf conf, std::vector<ProgramConf> programConfs)
  : Pman(conf)
{
  for (auto conf : programConfs) {
    programs.push_back(Program(conf));
  }
}

void Pman::daemonize()
{
  switch (fork()) {
    case -1: HANDLE_ERROR("fork");
    case 0: break;
    default: exit(0);
  }

  umask(0);

  if (chdir(conf.dir.c_str()) < 0) HANDLE_ERROR("chdir");

  if (setsid() < 0) HANDLE_ERROR("setsid");

  setRedirect(conf.logfile);
}

void Pman::registerAbrt()
{
  std::signal(SIGINT|SIGTERM|SIGQUIT, abrt_handler);
}

void Pman::registerSigchld()
{
  struct sigaction sa;
  sa.sa_handler = &sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART|SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) < 0) HANDLE_ERROR("sigaction");
}

void Pman::handleSigchld()
{
  Program *program;
  int killedPid;

  do {
    killedPid = waitpid(-1, 0, WNOHANG);
    if (killedPid > 0) {
      program = getProgram(killedPid);
      if (program) {
        program->stopped();
        LOG << "exited program " << program->name() << " pid " << killedPid << endl;

        if (program->autorestart()) {
          if (program->tooShort()) {
            LOG << "The process exited too quickly." << endl;
          } else {
            startProgram(*program);
          }
        }
      }
    }
  } while (killedPid > 0);
}

void Pman::startProgram(Program &program)
{
  int child_pid;
  switch (child_pid = fork()) {
    case -1: HANDLE_ERROR("fork");
    case 0:
      {
        setRedirect(program.logfile());

        int count = program.command().size();
        char *args[count + 1];
        for (int i = 0; i < count; i++) {
          args[i] = (char *)program.command().at(i).c_str();
        }
        args[count] = NULL;
        execv(args[0], args);
        perror(program.name().c_str());
        exit(1);
      }
    default:
      if (program.execCount() == 0) {
        LOG << "[Start] ";
      } else {
        LOG << "[Restart] ";
      }
      program.started(child_pid);
      cout << "program " << program.name() << " pid: " << child_pid << endl;
  }
}

void Pman::startAllPrograms()
{
  for (auto program = programs.begin(); program != programs.end(); ++program) {
    if (!program->isRunning()) startProgram(*program);
  }
}

Program *Pman::getProgram(int pid)
{
  for (auto program = programs.begin(); program != programs.end(); ++program) {
    if (program->pid() == pid) return &(* program);
  }
  return NULL;
}

void Pman::cleanup()
{
  for (auto program = programs.begin(); program != programs.end(); ++program) {
    if (program->isRunning()) {
      LOG << "Kill child process pid: " << program->pid() << endl;
      kill(program->pid(), SIGKILL);
    }
  }

  pidFile.remove();
}

int Pman::run()
{
  if (pidFile.check()) {
    cerr << "pman already exists" << endl;
    return 1;
  }

  daemonize();
  pidFile.write();
  registerAbrt();
  registerSigchld();

  LOG << "Start pman" << endl;

  startAllPrograms();

  while (!abrt_status) {
    sleep(2);

    if (sigchld_status) {
      handleSigchld();
      sigchld_status = 0;
    }
  }

  cleanup();
  LOG << "End pman" << endl;
  return 0;
}