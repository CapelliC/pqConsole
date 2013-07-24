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

/** enforce singleton handling
 */
SwiPrologEngine::SwiPrologEngine(ConsoleEdit *target, QObject *parent)
    : QThread(parent),
      FlushOutputEvents(target),
      argc(-1),
      thid(-1)
{
    Q_ASSERT(spe == 0);
    spe = this;
}

SwiPrologEngine::~SwiPrologEngine() {
    if (spe == this) {
        spe = 0;
        ready.wakeAll();

        bool ok = wait(1000);
        Q_ASSERT(ok);
    }
}

bool SwiPrologEngine::is_tty() {
    return PL_ttymode(Suser_input) == PL_RAWTTY;
}

void SwiPrologEngine::start(int argc, char **argv) {
    this->argv = new char*[this->argc = argc];
    for (int a = 0; a < argc; ++a)
        strcpy(this->argv[a] = new char[strlen(argv[a]) + 1], argv[a]);
    QThread::start();
}

void SwiPrologEngine::user_input(QString s) {
    buffer = s.toUtf8();
    ready.wakeOne();
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

_wait_:

    emit user_prompt(PL_thread_self(), is_tty());
    thid = -1;

    sync.lock();
    ready.wait(&sync);
    sync.unlock();

    if (!spe) // terminated
        return 0;

    // tag status running - and interruptable
    thid = PL_thread_self();

    // async query interface served from same thread
    if (!queries.empty()) {
        for ( ; !queries.empty(); ) {
            query p = queries.takeFirst();
            Q_ASSERT(!p.is_script);
            {
                QString n = p.name, t = p.text;
                try {
                    if (n.isEmpty()) {
                        PlQuery q("call", PlTermv(PlCompound(t.toUtf8())));
                        int occurrences = 0;
                        while (q.next_solution())
                            emit query_result(t, ++occurrences);
                        emit query_complete(t, occurrences);
                    }
                    else {
                        PlQuery q(A(n), "call", PlTermv(PlCompound(t.toUtf8())));
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
        }
        goto _wait_;
    }

    // resume IO
    uint n = buffer.length();
    Q_ASSERT(bufsize >= n);
    uint l = bufsize < n ? bufsize : n;
    qstrncpy(buf, buffer, l + 1);
    return l;
}

/** empty the buffer
 */
ssize_t SwiPrologEngine::_write_(void *handle, char *buf, size_t bufsize) {
    Q_UNUSED(handle);
    if (spe) {   // not terminated?
        emit spe->user_output(QString::fromUtf8(buf, bufsize));
        spe->flush();
    }
    return bufsize;
}

void SwiPrologEngine::run() {
    Sinput->functions->read = _read_;
    Soutput->functions->write = _write_;

    PL_set_prolog_flag("console_menu", PL_BOOL, TRUE);
    PL_set_prolog_flag("console_menu_version", PL_ATOM, "qt");

    PlEngine e(argc, argv);

    for (int a = 0; a < argc; ++a)
        delete [] argv[a];
    delete [] argv;
    argc = 0;

    PL_toplevel();
}

/** push an unnamed query, then issue delayed execution
 */
void SwiPrologEngine::query_run(QString text) {
    queries.append(query {false, "", text});
    ready.wakeOne();
}

/** push a named query, then issue delayed execution
 */
void SwiPrologEngine::query_run(QString module, QString text) {
    queries.append(query {false, module, text});
    ready.wakeOne();
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

/** issue user cancel request
 */
void SwiPrologEngine::cancel_running() {
    if (thid > 0) {
        qDebug() << "cancel_running";
        PL_thread_raise(thid, SIGINT);
    }
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
bool SwiPrologEngine::in_thread::named_load(QString n, QString t) {
    try {
        PlTerm cs, s, opts;
        if (    PlCall("atom_codes", PlTermv(A(t), cs)) &&
                PlCall("open_chars_stream", PlTermv(cs, s))) {
            PlTail l(opts);
            l.append(PlCompound("stream", PlTermv(s)));
            l.append(PlCompound("silent", PlTermv(A("true"))));
            l.close();
            if (PlCall("load_files", PlTermv(A(n), opts))) {
                PlCall("close", PlTermv(s));
                return true;
            }
        }
    }
    catch(PlException ex) {
        qDebug() << CCP(ex);
    }
    return false;
}
