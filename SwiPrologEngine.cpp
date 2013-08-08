/*
    pqConsole    : interfacing SWI-Prolog and Qt

    Author       : Carlo Capelli
    E-mail       : cc.carlo.cap@gmail.com
    Copyright (C): 2013, Carlo Capelli

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
#include <signal.h>
#include <QTimer>

/** singleton handling - process main engine
 */
SwiPrologEngine *SwiPrologEngine::spe;
static PlEngine *ple;

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
bool SwiPrologEngine::is_tty() {
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

/** from console fron end: user - or a equivalent actor - has input s
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

    emit user_prompt(PL_thread_self(), is_tty());

    for ( ; ; ) {

        {   QMutexLocker lk(&sync);

            if (!spe) // terminated
                return 0;

            if (!queries.empty())
                serve_query(queries.takeFirst());

            //qDebug() << "loop" << spe->target->status;
            if (spe->target->status == ConsoleEdit::closing) {
                qDebug() << "spe->target->status == ConsoleEdit::closing" << CVP(CT);
#if 0
                if (PlCall("current_prolog_flag(xpce, true)")) {
                    qDebug() << "current_prolog_flag(xpce, true)";
                    /*
                    if (PlCall("send(@pce, die, 0)")) {
                        qDebug() << "send(@pce, die, 0)";
                        for (int n = 0; n < 10; ++n) {
                            if (PlCall("current_prolog_flag(xpce, true)")) {
                                //msleep(1000);
                                qDebug() << "do_events(1000)";
                                do_events(1000);
                            }
                        }
                        return 0;
                    }
                    else
                        qDebug() << "send failed";
                    */
                    buffer = "send(@pce, die, 0).\n";
                }
                else
                    return 0;
#endif
                return 0;
            }

            uint n = buffer.length();
            if (n > 0) {
                Q_ASSERT(bufsize >= n);
                uint l = bufsize < n ? bufsize : n;
                qstrncpy(buf, buffer, l + 1);
                buffer.clear();
                return l;
            }
        }

        msleep(100);
    }
}

/** async query interface served from same thread
 */
void SwiPrologEngine::serve_query(query p) {
    Q_ASSERT(!p.is_script);
    QString n = p.name, t = p.text;
    try {
        if (n.isEmpty()) {
            PlQuery q("call", PlTermv(PlCompound(t.toUtf8())));
            //PlQuery q("call", PlTermv(PlCompound(t.toStdWString().data())));
            int occurrences = 0;
            while (q.next_solution())
                emit query_result(t, ++occurrences);
            emit query_complete(t, occurrences);
        }
        else {
            PlQuery q(A(n), "call", PlTermv(PlCompound(t.toUtf8())));
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

void SwiPrologEngine::run() {
    Sinput->functions->read = _read_;
    Soutput->functions->write = _write_;

    PL_set_prolog_flag("console_menu", PL_BOOL, TRUE);
    PL_set_prolog_flag("console_menu_version", PL_ATOM, "qt");

    target->add_thread(1);
    ple = new PlEngine(argc, argv);

    for (int a = 0; a < argc; ++a)
        delete [] argv[a];
    delete [] argv;
    argc = 0;

    PL_toplevel();

    qDebug() << "spe" << CVP(spe);
    if (spe && spe->target->status == ConsoleEdit::closing) {
        delete ple;
        ple = 0;
    }
    spe = 0;
}

/** push an unnamed query, thus unlocking the execution polling loop
 */
void SwiPrologEngine::query_run(QString text) {
    QMutexLocker lk(&sync);
    queries.append(query {false, "", text});
}

/** push a named query, thus unlocking the execution polling loop
 */
void SwiPrologEngine::query_run(QString module, QString text) {
    QMutexLocker lk(&sync);
    queries.append(query {false, module, text});
}

/** allows to run a delayed script from resource at startup
 */
void SwiPrologEngine::script_run(QString name, QString text) {
    queries.append(query {true, name, text});
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

/** attaching *main* thread engine to another thread (ok GUI thread)
 */
SwiPrologEngine::in_thread::in_thread()
    : frame(0)
{
    while (!spe)
        msleep(100);
    while (!spe->isRunning())
        msleep(100);
    while (spe->argc)
        msleep(100);

    int id = PL_thread_attach_engine(0);
    Q_ASSERT(id >= 0);
    frame = new PlFrame;
}

SwiPrologEngine::in_thread::~in_thread() {
    delete frame;
    PL_thread_destroy_engine();
}

/** run script <t>, named <n> in current thread
 */
bool SwiPrologEngine::in_thread::named_load(QString n, QString t, bool silent) {
    try {
        PlTerm cs, s, opts;
        if (    PlCall("atom_codes", PlTermv(A(t), cs)) &&
                PlCall("open_chars_stream", PlTermv(cs, s))) {
            PlTail l(opts);
            l.append(PlCompound("stream", PlTermv(s)));
            if (silent)
                l.append(PlCompound("silent", PlTermv(A("true"))));
            l.close();
            bool rc = PlCall("load_files", PlTermv(A(n), opts));
            PlCall("close", PlTermv(s));
            return rc;
        }
    }
    catch(PlException ex) {
        qDebug() << CCP(ex);
    }
    return false;
}

/** handle application quit request in thread that started PL_toplevel
 *  logic moved here from pqMainWindow
 */
bool SwiPrologEngine::quit_request() {
    in_thread e;
    if (PlCall("current_prolog_flag(xpce, true)"))
        PlCall("send(@pce, die, 0)");
    else if (spe) {
        spe->target->status = ConsoleEdit::closing;
        for (int n = 0; n < 10; ++n) {
            if (!spe)
                break;
            msleep(100);
        }
    }

    return true;
}
