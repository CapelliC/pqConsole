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

#include "SwiPrologEngine.h"
#include "PREDICATE.h"
#include <SWI-cpp.h>
#include <SWI-Stream.h>
#include <QtDebug>
#include <signal.h>
#include <QTimer>

/** singleton handling - process main engine
 */
SwiPrologEngine *SwiPrologEngine::spe;

/** enforce singleton handling
 */
SwiPrologEngine::SwiPrologEngine(QObject *parent)
    : QThread(parent),
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

/** attaching *main* thread engine to another thread (ok GUI thread)
 */
SwiPrologEngine::in_thread::in_thread() : frame(0) {
    while (!spe)
        msleep(100);
    while (!spe->isRunning())
        msleep(100);
    while (spe->argc)
        msleep(100);
    PL_thread_attach_engine(0);
    frame = new PlFrame;
}

SwiPrologEngine::in_thread::~in_thread() {
    delete frame;
    PL_thread_destroy_engine();
}

void SwiPrologEngine::start(int argc, char **argv) {
    this->argv = new char*[this->argc = argc];
    for (int a = 0; a < argc; ++a)
        strcpy(this->argv[a] = new char[strlen(argv[a]) + 1], argv[a]);
    QThread::start();
}

void SwiPrologEngine::user_input(QString s) {
    //qDebug() << "user_input available" << CVP(this) << CVP(CT);
    buffer = s.toUtf8();
    ready.wakeOne();
}

/** fill the buffer
 */
ssize_t SwiPrologEngine::_read_(void *handle, char *buf, size_t bufsize) {
    //qDebug() << "_read_" << CVP(handle) << CVP(CT);
    Q_UNUSED(handle);
    Q_ASSERT(spe);
    return spe->_read_(buf, bufsize);
}

/** background read & query loop
 */
ssize_t SwiPrologEngine::_read_(char *buf, size_t bufsize) {

_wait_:

    //qDebug() << "POLL" << CVP(this) << CVP(CT);

    emit user_prompt(PL_thread_self());
    thid = -1;

    sync.lock();
    //bool x =
    ready.wait(&sync);
    //qDebug() << "ready" << x;
    sync.unlock();

    if (!spe) // terminated
        return 0;

    //qDebug() << "AWAKE" << x << CVP(this) << CVP(CT);

    // tag status running - and interruptable
    thid = PL_thread_self();

    // async query interface served from same thread
    if (!queries.empty()) {
        for ( ; !queries.empty(); ) {
            query p = queries.takeFirst();
            Q_ASSERT(!p.is_script);
            {
                QString n = p.name, t = p.text;
                qDebug() << "n:" << n << "t:" << t;
                try {
                    PlQuery q(A(n), "call", PlTermv(PlCompound(t.toUtf8())));
                    int occurrences = 0;
                    while (q.next_solution())
                        emit query_result(t, ++occurrences);
                    emit query_complete(t, occurrences);
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
    //if (handle == CVP(1)) qDebug() << "here is prompt";

    //qDebug() << "_write_" << CVP(handle) << CVP(CT) << QString::fromUtf8(buf, bufsize);
    Q_UNUSED(handle);
    if (spe)    // not terminated?
        emit spe->user_output(QString::fromUtf8(buf, bufsize));
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

void SwiPrologEngine::query_run(QString text) {
    queries.append(query{false, "", text});
    ready.wakeOne();
    //qDebug() << "query_run::wakeOne" << CVP(this) << CVP(CT);
}
void SwiPrologEngine::query_run(QString module, QString text) {
    queries.append(query{false, module, text});
    ready.wakeOne();
    //qDebug() << "query_run::wakeOne" << CVP(this) << CVP(CT);
}
void SwiPrologEngine::script_run(QString name, QString text) {
    //qDebug() << "script_run" << CVP(spe) << CVP(CT);
    queries.append(query{true, name, text});
    QTimer::singleShot(100, this, SLOT(awake()));
}
void SwiPrologEngine::awake() {
    Q_ASSERT(queries.count() == 1);
    query p = queries.takeFirst();
    QString n = p.name, t = p.text;
    Q_ASSERT(!n.isEmpty());
    in_thread I;
    try {
        T cs, s, opts;
        if (    PlCall("atom_codes", V(A(t), cs)) &&
                PlCall("open_chars_stream", V(cs, s))) {
            L l(opts);
            l.append(C("stream", V(s)));
            l.close();
            if (PlCall("load_files", V(A(n), opts))) {
                PlCall("close", V(s));
                return;
            }
        }
    }
    catch(PlException ex) {
        qDebug() << CCP(ex);
    }
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
