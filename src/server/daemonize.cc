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
#include <base/os_error.h>
#include <base/zero.h>

using namespace Base;

/* Install the given hander over a list of signals. */
static void InstallSignalHandlers(std::initializer_list<int> signals,
    void (*handler)(int)) {
  for (int sig_num : signals) {
    struct sigaction new_act;
    Base::Zero(new_act);
    new_act.sa_handler = handler;
    IfLt0(sigaction(sig_num, &new_act, nullptr));
  }
}

pid_t Server::Daemonize() {
  pid_t result;

  /* Check our parent's PID to see if we're already a daemon. */
  if (getppid() != 1) {
    /* We're not already a daemon. */
    TOsError::IfLt0(HERE, result = fork());
    /* We've forked.  If we're the daemon child... */

    if (!result) {
      /* Obtain a new process group. */
      setsid();
      /* Close stdin, stdout, and stderr. */
      close(0);
      close(1);
      close(2);
      /* Reroute stdin and stdout to dev/null. */
      int dev_null = open("/dev/null", O_RDWR);
      IfLt0(dup(dev_null));
      IfLt0(dup(dev_null));
      /* Set newly created file permissions. */
      umask(0);
      /* Move to the root dir. */
      IfLt0(chdir("/"));
      /* Keep signals away. */
      DefendAgainstSignals();
    }
  } else {
    /* We're already a daemon. */
    result = 0;
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
