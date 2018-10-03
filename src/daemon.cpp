#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "defines.h"
#include "daemon.hpp"

using namespace std;

extern mutex g_mtx;
extern bool g_ready;

namespace {
  volatile std::sig_atomic_t abrt_status;
  volatile std::sig_atomic_t sigchld_status;
}
void abrt_handler(int signal) { abrt_status = signal; };
void sigchld_handler(int signal) { sigchld_status = signal; };

Daemon::Daemon(PmanConf conf, std::vector<ProgramConf> programConfs)
  : conf_(conf), pidFile_(PidFile(conf.pidfile))
{
  for (auto pConf : programConfs) {
    this->programs_.push_back(Program(pConf));
  }
}

void Daemon::daemonize()
{
  switch (fork()) {
    case -1: HANDLE_ERROR("fork");
    case 0: break;
    default: exit(0);
  }

  umask(0);

  if (chdir(this->conf_.dir.c_str()) < 0) HANDLE_ERROR("chdir");

  if (setsid() < 0) HANDLE_ERROR("setsid");

  setRedirect(this->conf_.logfile);
}

void Daemon::registerAbrt()
{
  std::signal(SIGINT|SIGTERM|SIGQUIT, abrt_handler);
}

void Daemon::registerSigchld()
{
  struct sigaction sa;
  sa.sa_handler = &sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART|SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) < 0) HANDLE_ERROR("sigaction");
}

void Daemon::handleSigchld()
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

void Daemon::startProgram(string name)
{
  Program *p = getProgram(name);
  if (p) startProgram(*p);
}

void Daemon::startProgram(Program &program)
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

void Daemon::startAllPrograms()
{
  for (auto program = this->programs_.begin(); program != this->programs_.end(); ++program) {
    if (program->isRunning()) {
      LOG << "program " << program->name() << " is already running." << endl;
    } else {
      startProgram(*program);
    }
  }
}

Program *Daemon::getProgram(std::string name)
{
  for (auto program = this->programs_.begin(); program != this->programs_.end(); ++program) {
    if (program->name() == name) return &(* program);
  }
  return NULL;
}

Program *Daemon::getProgram(int pid)
{
  for (auto program = this->programs_.begin(); program != this->programs_.end(); ++program) {
    if (program->pid() == pid) return &(* program);
  }
  return NULL;
}

void Daemon::cleanup()
{
  for (auto program = this->programs_.begin(); program != this->programs_.end(); ++program) {
    if (program->isRunning()) {
      LOG << "Kill child process pid: " << program->pid() << endl;
      kill(program->pid(), SIGKILL);
    }
  }

  this->pidFile_.remove();
}

void Daemon::setup()
{
  if (this->pidFile_.check()) {
    cerr << "pman daemon is already running (pid: " << this->pidFile_.read() << ")" << endl;
    exit(1);
  }

  daemonize();
  this->pidFile_.write();
  registerAbrt();
  registerSigchld();
}

int Daemon::runLoop()
{

  LOG << "Start pman" << endl;

  while (!abrt_status) {
    sleep(2);

    {
      lock_guard<mutex> lk(g_mtx);
      while (!this->tasks_.empty()) {
        Task *task = this->tasks_.front();
        if (task->op == Task::Operation::START) {
          if (task->name == "all") {
            startAllPrograms();
          } else {
            startProgram(task->name);
          }
        } else if (task->op == Task::Operation::STOP) {
          // - TODO: stop process
          LOG << "unimplemented stop" << endl;
        }

        g_ready = true;
        this->tasks_.pop();
        task->cv.notify_one();
      }
    }

    if (sigchld_status) {
      lock_guard<mutex> lk(g_mtx);
      handleSigchld();
      sigchld_status = 0;
    }
  }

  cleanup();
  LOG << "End pman" << endl;
  return 0;
}
