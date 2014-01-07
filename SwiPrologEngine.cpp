/*
    pqConsole    : interfacing SWI-Prolog and Qt

    Author       : Carlo Capelli
    E-mail       : cc.carlo.cap@gmail.com
    Copyright (C): 2013,2014 Carlo Capelli

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <SWI-Stream.h>
#include <SWI-cpp.h>
#include "SwiPrologEngine.h"
#include "PREDICATE.h"

#include "ConsoleEdit.h"
#include "do_events.h"

#include <QtDebug>
#include <QApplication>
#include <signal.h>
#include <QTimer>
#include <QFile>

/** singleton handling - process main engine
 */
SwiPrologEngine *SwiPrologEngine::spe;

/** enforce singleton handling
 */
SwiPrologEngine::SwiPrologEngine(ConsoleEdit *target, QObject *parent)
    : QThread(parent),
      FlushOutputEvents(target),
      argc(-1)
{
    Q_ASSERT(spe == 0);
    spe = this;
}

/** enforce proper termination sequence
 */
SwiPrologEngine::~SwiPrologEngine() {
    Q_ASSERT(spe == 0);
}

/** check stream property
 */
bool SwiPrologEngine::is_tty(const FlushOutputEvents *f) { Q_UNUSED(f)
 // qDebug() << CVP(Suser_input) << "tty" << (PL_ttymode(Suser_input) == PL_RAWTTY);
    return PL_ttymode(Suser_input) == PL_RAWTTY;
}

/** background thread setup
 */
void SwiPrologEngine::start(int argc, char **argv) {
    this->argv = new char*[this->argc = argc];
    for (int a = 0; a < argc; ++a)
        strcpy(this->argv[a] = new char[strlen(argv[a]) + 1], argv[a]);
    QThread::start();
}

/** from console front end: user - or a equivalent actor - has input s
 */
void SwiPrologEngine::user_input(QString s) {
    QMutexLocker lk(&sync);
    buffer = s.toUtf8();
}

/** fill the buffer
 */
ssize_t SwiPrologEngine::_read_(void *handle, char *buf, size_t bufsize) {
    Q_UNUSED(handle);
    Q_ASSERT(spe);
    return spe->_read_(buf, bufsize);
}

/** background read & query loop
 */
ssize_t SwiPrologEngine::_read_(char *buf, size_t bufsize) {

    if (buffer.isEmpty())
        emit user_prompt(PL_thread_self(), is_tty(this));

    for ( ; ; ) {

        {   QMutexLocker lk(&sync);

            if (!spe) // terminated
                return 0;

            if (!queries.empty())
                serve_query(queries.takeFirst());

            uint n = buffer.length();
            if (n > 0) {
                uint l = bufsize < n ? bufsize : n;
                memcpy(buf, buffer, l);
                buffer.remove(0, l);
                return l;
            }

            if (target->status == ConsoleEdit::eof) {
                target->status = ConsoleEdit::running;
                return 0;
            }
        }

        if (PL_handle_signals() < 0)
            return -1;

        msleep(100);
    }
}

/** async query interface served from same thread
 */
void SwiPrologEngine::serve_query(query p) {

    query1(call)

    Q_ASSERT(!p.is_script);
    QString n = p.name, t = p.text;
    try {
        if (n.isEmpty()) {
            //PlQuery q("call", PlTermv(PlCompound(t.toUtf8())));
            call q(C(t.toUtf8()));
            //PlQuery q("call", PlTermv(PlCompound(t.toStdWString().data())));
            int occurrences = 0;
            while (q.next_solution())
                emit query_result(t, ++occurrences);
            emit query_complete(t, occurrences);
        }
        else {
            //PlQuery q(A(n), "call", PlTermv(PlCompound(t.toUtf8())));
            call q(C(t.toUtf8()));
            //PlQuery q(A(n), "call", PlTermv(PlCompound(t.toStdWString().data())));
            int occurrences = 0;
            while (q.next_solution())
                emit query_result(t, ++occurrences);
            emit query_complete(t, occurrences);
        }
    }
    catch(PlException ex) {
        qDebug() << t << CCP(ex);
        emit query_exception(n, CCP(ex));
    }
}

/** empty the buffer
 */
ssize_t SwiPrologEngine::_write_(void *handle, char *buf, size_t bufsize) {
    Q_UNUSED(handle);
    if (spe) {   // not terminated?
        emit spe->user_output(QString::fromUtf8(buf, bufsize));
        if (spe->target->status == ConsoleEdit::running)
            spe->flush();
    }
    return bufsize;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
The role of this function is to stop  changing the encoding of the plwin
output. We must return -1 for  SIO_SETENCODING   for  this. As we do not
implement any of the other control operations   we  simply return -1 for
all commands we may be requested to handle.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int SwiPrologEngine::_control_(void *handle, int cmd, void *closure)
{ Q_UNUSED(handle);
  Q_UNUSED(cmd);
  Q_UNUSED(closure);
  return -1;
}


int SwiPrologEngine::halt_engine(int status, void*data)
{ Q_UNUSED(data);

  qDebug() << "halt_engine" << status;
  QCoreApplication::quit();
  msleep(5000);

  return 0;
}

static IOFUNCTIONS pq_functions;

void SwiPrologEngine::run() {
    pq_functions         = *Sinput->functions;
    pq_functions.read    = _read_;
    pq_functions.write	 = _write_;
 // pq_functions.close   = _close_; /* JW: might be needed.  See pl-ntmain.c */
    pq_functions.control = _control_;

    Sinput->functions  = &pq_functions;
    Soutput->functions = &pq_functions;
    Serror->functions  = &pq_functions;

    Sinput->flags  |= SIO_ISATTY;
    Soutput->flags |= SIO_ISATTY;
    Serror->flags  |= SIO_ISATTY;

    Sinput->encoding  = ENC_UTF8; /* is this correct? */
    Soutput->encoding = ENC_UTF8;
    Serror->encoding  = ENC_UTF8;

    Sinput->flags  &= ~SIO_FILE;
    Soutput->flags &= ~SIO_FILE;
    Serror->flags  &= ~SIO_FILE;

    PL_set_prolog_flag("console_menu", PL_BOOL, TRUE);
    PL_set_prolog_flag("console_menu_version", PL_ATOM, "qt");
    PL_set_prolog_flag("xpce_threaded", PL_BOOL, TRUE);

    target->add_thread(1);
    PL_exit_hook(halt_engine, NULL);

    PL_initialise(argc, argv);

    // use as initialized flag
    argc = 0;

    {   PlTerm color_term;
        if (PlCall("current_prolog_flag", PlTermv("color_term", color_term)) && color_term == "false")
            target->color_term = false;
    }

    for ( ; ; ) {
        int status = PL_toplevel() ? 0 : 1;
        qDebug() << "PL_halt" << status;
        PL_halt(status);
    }
}

/** push an unnamed query, thus unlocking the execution polling loop
 */
void SwiPrologEngine::query_run(QString text) {
    QMutexLocker lk(&sync);
#if !defined(_MSC_VER) || _MSC_VER >= 1800
    queries.append(query {false, "", text});
#else
    queries.append(query(false, "", text));
#endif
}

/** push a named query, thus unlocking the execution polling loop
 */
void SwiPrologEngine::query_run(QString module, QString text) {
    QMutexLocker lk(&sync);
#if !defined(_MSC_VER) || _MSC_VER >= 1800
    queries.append(query {false, module, text});
#else
    queries.append(query(false, module, text));
#endif
}

/** allows to run a delayed script from resource at startup
 */
void SwiPrologEngine::script_run(QString name, QString text) {
#if !defined(_MSC_VER) || _MSC_VER >= 1800
    queries.append(query {true, name, text});
#else
    queries.append(query(true, name, text));
#endif
    QTimer::singleShot(100, this, SLOT(awake()));
}
void SwiPrologEngine::awake() {
    Q_ASSERT(queries.count() == 1);
    query p = queries.takeFirst();
    Q_ASSERT(!p.name.isEmpty());
    in_thread I;
    if (!I.named_load(p.name, p.text))
        qDebug() << "awake failed";
}

/** Create a Prolog thread for the GUI thread, so we can call Prolog
    goals.  These engines are created to deal with call-backs from the
    gui and destroyed after the callback has finished. This is used only
    if the thread associated to the current tab is not running a query.
 */
SwiPrologEngine::in_thread::in_thread()
    : frame(0), thid(-1)
{
    if (PL_thread_self() == -1) {
        // no thread yet available
        while (!spe)
            msleep(100);
        while (!spe->isRunning())
            msleep(100);
        while (spe->argc)
            msleep(100);

        PL_thread_attr_t attr;
        memset(&attr, 0, sizeof(attr));
        attr.flags = PL_THREAD_NO_DEBUG;

        // CC: aliasing should have a *different* name for different threads...
        //attr.alias = (char*)"__gui";
        QByteArray alias;
        QTextStream(&alias) << "_gt_" << QThread::currentThreadId();
        attr.alias = alias.data();

        qDebug() << "in_thread:PL_thread_attach_engine" << CT;

        thid = PL_thread_attach_engine(&attr);
        Q_ASSERT(thid >= 0);			/* JW: Should throw exception */
    }

    frame = new PlFrame;
}

SwiPrologEngine::in_thread::~in_thread() {
    delete frame;
    if (thid != -1)
        PL_thread_destroy_engine();
}

structure1(stream)
structure1(silent)

predicate2(atom_codes)
predicate2(open_chars_stream)
predicate2(load_files)
predicate1(current_module)
predicate1(close)

bool SwiPrologEngine::named_load(QString n, QString t, bool silent_yn) {
    //qDebug() << "SwiPrologEngine::named_load" << n << t.length() << t << silent_yn;
    try {
        PlTerm cs, s, opts;
        if (    atom_codes(A(t), cs) &&
                open_chars_stream(cs, s)) {
            PlTail l(opts);
            l.append(stream(s));
            if (silent_yn)
                l.append(silent(A("true")));
            l.close();
            //bool rc = load_files(A(n), opts);
            bool rc = PlCall("user", "load_files", V(A(n), opts));
            close(s);
            return rc;
        }
    }
    catch(PlException ex) {
        qDebug() << t2w(ex);
    }
    return false;
}

/** run script <t>, named <n> in current thread
 */
bool SwiPrologEngine::in_thread::named_load(QString n, QString t, bool silent_yn) {
    return SwiPrologEngine::named_load(n, t, silent_yn);
}

/** if module not yet loaded, load code (i.e. assumes it starts with :-module(module))
 */
bool SwiPrologEngine::in_thread::inline_module(QString module, QString  code, bool silent) {
    if (!current_module(A(module))) {
        qDebug() << "loading module snippet" << module;
        return named_load(module, code, silent);
    }
    qDebug() << "module available" << module;
    return true;
}

/** if not yet loaded, parse module code from resource
 */
bool SwiPrologEngine::in_thread::resource_module(QString module, QString location, bool silent) {
    if (!current_module(A(module))) {
        qDebug() << "loading resource_module" << module << "from" << location;
        QString path = location + "/" + module + ".pl";
        QFile file(path);
        if (!file.open(file.ReadOnly | file.Text)) {
            qDebug() << "path not found" << path;
            return false;
        }
        return named_load(path, file.readAll(), silent);
    }
    qDebug() << "module available" << module;
    return true;
}

/** handle application quit request in thread that started PL_toplevel
 *  logic moved here from pqMainWindow
 */
bool SwiPrologEngine::quit_request() {
    qDebug() << "quit_request; spe = " << spe;
    if (spe)
        spe->query_run("halt");
    else
        PL_halt(0);
    return false;
}
