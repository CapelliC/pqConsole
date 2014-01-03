pqConsole:

  a basic Console, running SWI-Prolog PlEngine in background,
  presenting a QPlainTextEditor for user interface in foreground (GUI) thread.

Features

 - handling of keyboard input specialized for Prolog REPL
   and integration in TAB based multiwindow interfaces
 - output text colouring (subset of ANSI terminal sequences)
 - commands history
 - completion interface
 - swipl-win compatible API, allows menus to be added to top level widget,
   and enable creating a console for each thread
 - XPCE ready, allows reuse of current IDE components

History

 - pqConsole sources have been embedded in swipl-win, hosted on SWI-Prolog git official repository.
   Since I'm still interested in keeping pqConsole independent from actual applicative front end,
   from time to time there will be a manual sync, to keep this one aligned.

author:  Carlo Capelli - Brescia 2013,2014
licence: LGPL v2.1
