/* <server/daemonize.cc>

   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   Implements <server/daemonize.h>.
 */

#include <server/daemonize.h>

#include <initializer_list>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/wr/fd_util.h>
#include <base/wr/file_util.h>
#include <base/wr/process_util.h>
#include <base/wr/signal_util.h>
#include <base/zero.h>

using namespace Base;

/* Install the given hander over a list of signals. */
static void InstallSignalHandlers(std::initializer_list<int> signals,
    void (*handler)(int)) {
  for (int sig_num : signals) {
    struct sigaction new_act;
    Base::Zero(new_act);
    new_act.sa_handler = handler;
    Wr::sigaction(sig_num, &new_act, nullptr);
  }
}

pid_t Server::Daemonize() {
  if (getppid() == 1) {
    return 0;  // We're already a daemon.
  }

  const pid_t result = Wr::fork();
  assert(result >= 0);

  if (!result) {
    /* Obtain a new process group. */
    setsid();

    /* Reroute stdin, stdout, and stderr to dev/null. */
    const int dev_null = Wr::open(Wr::TDisp::Nonfatal, {}, "/dev/null",
        O_RDWR);
    assert(dev_null >= 0);
    int ret = Wr::dup2(dev_null, 0);
    assert(ret == 0);
    ret = Wr::dup2(dev_null, 1);
    assert(ret == 1);
    ret = Wr::dup2(dev_null, 2);
    assert(ret == 2);

    if (dev_null > 2) {
      Wr::close(dev_null);
    }

    /* Set newly created file permissions. */
    umask(0);

    /* Move to the root dir. */
    Wr::chdir(Wr::TDisp::Nonfatal, {}, "/");

    /* Keep signals away. */
    DefendAgainstSignals();
  }

  return result;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
void Server::DefendAgainstSignals() {
  InstallSignalHandlers({ SIGCHLD, SIGTSTP, SIGTTOU, SIGTTIN, SIGHUP },
      SIG_IGN);
}
#pragma GCC diagnostic pop
