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
#include <SWI-cpp.h>
#include <SWI-Stream.h>
#include <QtDebug>
#include <signal.h>

// singleton handling - just 1 engine x process
static SwiPrologEngine *spe;

SwiPrologEngine::SwiPrologEngine(QObject *parent) : QThread(parent), argc(-1), thid(-1) {
    // singleton handling
    Q_ASSERT(spe == 0);
    spe = this;
}

SwiPrologEngine::~SwiPrologEngine() {
    spe = 0;
    ready.wakeAll();

    bool ok = wait(1000);
    Q_ASSERT(ok);
}

SwiPrologEngine::in_thread::in_thread() {
    while (!spe)
        msleep(100);
    while (!spe->isRunning())
        msleep(100);
    while (spe->argc)
        msleep(100);
    PL_thread_attach_engine(0);
}
SwiPrologEngine::in_thread::~in_thread() {
    PL_thread_destroy_engine();
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

/* fill the buffer */
ssize_t SwiPrologEngine::_read_(void *handle, char *buf, size_t bufsize) {
    Q_UNUSED(handle);
    Q_ASSERT(spe);
    return spe->_read_(buf, bufsize);
}

/* background read & query loop */
ssize_t SwiPrologEngine::_read_(char *buf, size_t bufsize) {
    emit user_prompt();

    thid = -1;

 _wait_:
    ready.wait(&sync);
    if (!spe) // terminated
        return 0;

    // tag status running - and interruptable
    thid = PL_thread_self();

    // async query interface served from same thread
    if (!queries.empty()) {
        for ( ; !queries.empty(); ) {
            QString t = queries.takeFirst();
            try {
                PlQuery q("call", PlTermv(PlCompound(t.toUtf8())));
                int occurrences = 0;
                while (q.next_solution())
                    emit query_result(t, ++occurrences);
                emit query_complete(t, occurrences);
            }
            catch(PlException ex) {
                const char *msg = ex;
                emit query_exception(t, msg);
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

/* empty the buffer */
ssize_t SwiPrologEngine::_write_(void *handle, char *buf, size_t bufsize) {
    Q_UNUSED(handle);
    if (spe)    // not terminated?
        emit spe->user_output(QString::fromUtf8(buf, bufsize));
    return bufsize;
}

void SwiPrologEngine::run() {
    Sinput->functions->read = _read_;
    Soutput->functions->write = _write_;

    PlEngine e(argc, argv);

    for (int a = 0; a < argc; ++a)
        delete [] argv[a];
    delete [] argv;
    argc = 0;

    PL_toplevel();
}

void SwiPrologEngine::query_run(QString text) {
    queries.append(text);
    ready.wakeOne();
}

// issue user cancel request
void SwiPrologEngine::cancel_running() {
    if (thid > 0) {
        qDebug() << "cancel_running";
        PL_thread_raise(thid, SIGINT);
    }
}
