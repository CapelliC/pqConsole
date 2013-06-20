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

#ifndef CONSOLEEDIT_H
#define CONSOLEEDIT_H

#include <QPlainTextEdit>
#include "SwiPrologEngine.h"

typedef QPlainTextEdit ConsoleEditBase;

// client side of command line interface
// run in GUI thread, sync using SwiPrologEngine interface
//
class PQCONSOLESHARED_EXPORT ConsoleEdit : public ConsoleEditBase {
    Q_OBJECT

public:

    // build command line interface to SWI Prolog engine
    ConsoleEdit(int argc, char **argv, QWidget *parent = 0);

    // push command on queue
    bool command(QString text);

    // access low level interface
    SwiPrologEngine* engine() { return &eng; }

    // SINGLETON pattern - only a console allowed in a process
    static ConsoleEdit* console();

protected:

    // host actual interface object, running in background
    SwiPrologEngine eng;
    enum {startup, running} status;

    // strict control on keyboard events required
    virtual void keyPressEvent(QKeyEvent *event);

    // output text attributes
    QTextCharFormat tcf;

    // start point of engine output insertion
    // i.e. keep last user editable position
    int fixedPosition;

    // commands to be dispatched to engine thread
    QStringList commands;

    // poor man command history
    QStringList history;
    int history_next;

    // count output before setting cursor at end
    int count_output;

public slots:

    // send text to output
    void out(QString text);

    // issue an input request
    void prompt();

    // display different cursor where editing available
    void onCursorPositionChanged();

protected slots:

    // push command from queue to Prolog processor
    void command_do();

signals:

    // issued to serve prompt
    void inp(QString);
};

#endif
