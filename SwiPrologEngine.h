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

#ifndef SWIPROLOGENGINE_H
#define SWIPROLOGENGINE_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QVariant>
#include <QStringList>

#include "pqConsole_global.h"

// interface IO running SWI Prolog engine in background
//
class PQCONSOLESHARED_EXPORT SwiPrologEngine : public QThread {
    Q_OBJECT
public:

    explicit SwiPrologEngine(QObject *parent = 0);
    ~SwiPrologEngine();

    // main console startup point
    void start(int argc, char **argv);

    // run query on background thread
    void query_run(QString text);

    // issue user cancel request
    void cancel_running();

    // start/stop a Prolog engine in thread - use for syncronized GUI
    struct in_thread { in_thread(); ~in_thread(); };

signals:

    // issued to queue a string to user output
    void user_output(QString output);

    // issued to peek input - til to CR - from user
    void user_prompt();

    // signal a query result
    void query_result(QString query, int occurrence);

    // signal query completed
    void query_complete(QString query, int tot_occurrences);

    // signal exception
    void query_exception(QString query, QString message);

public slots:
    void user_input(QString intput);

protected:
    virtual void run();

    int argc;
    char **argv;

    QMutex sync;
    QByteArray buffer;
    QWaitCondition ready;

    // queries to be dispatched to engine thread
    QStringList queries;

    static ssize_t _read_(void *handle, char *buf, size_t bufsize);
    static ssize_t _write_(void *handle, char *buf, size_t bufsize);
    ssize_t _read_(char *buf, size_t bufsize);

    // background thread ID
    int thid;
};

#endif // SWIPROLOGENGINE_H
