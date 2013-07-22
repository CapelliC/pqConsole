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
#include <QCompleter>

typedef QPlainTextEdit ConsoleEditBase;

#include "SwiPrologEngine.h"
#include "Completion.h"

class Swipl_IO;

/** client side of command line interface
  * run in GUI thread, sync using SwiPrologEngine interface
  */
class PQCONSOLESHARED_EXPORT ConsoleEdit : public ConsoleEditBase {
    Q_OBJECT
    Q_PROPERTY(int updateRefreshRate READ updateRefreshRate WRITE setUpdateRefreshRate)

public:

    /** build command line interface to SWI Prolog engine */
    ConsoleEdit(int argc, char **argv, QWidget *parent = 0);

    /** create in prolog thread - call win_open_console() */
    ConsoleEdit(Swipl_IO* io, QString Title);

    /** create in prolog thread - from win_open_console(), add to tabbed interface */
    ConsoleEdit(Swipl_IO* io);

    /** push command on queue */
    bool command(QString text);

    /** access low level interface */
    SwiPrologEngine* engine() { return eng; }

    /** a console is associated with a worker Prolog thread
     *  should handle the case of yet-to-be-initialized root console
     */
    bool match_thread(int thread_id) const { return thid == thread_id || thid == -1 || thread_id == -1; }

    /** remove all text */
    void tty_clear();

    /** make public property, then available on Prolog side */
    int updateRefreshRate() const { return update_refresh_rate; }
    void setUpdateRefreshRate(int v) { update_refresh_rate = v; }

    /** create a new console, bound to calling thread */
    void new_console(Swipl_IO *e, QString title);

    /** closeEvent only called for top level widgets */
    bool can_close();

    /** 4. attempt to run generic code inter threads */
    void exec_func(pfunc f) { emit sig_run_function(f); }

    /** 5. helper syncronization for modal loop */
    struct exec_sync {
        QMutex sync;
        QWaitCondition ready;
        void stop() {
            sync.lock();
            ready.wait(&sync);
        }
        void go() {
            ready.wakeOne();
        }
    };

    /** give access to rl_... predicates */
    const QStringList& history_lines() const { return history; }
    void add_history_line(QString line);

protected:

    /** host actual interface object, running in background */
    SwiPrologEngine *eng;

    /** can't get <eng> to work on a foreign thread - initiated from SWI-Prolog */
    Swipl_IO *io;

    /** strict control on keyboard events required */
    virtual void keyPressEvent(QKeyEvent *event);

    /** support completion */
    virtual void focusInEvent(QFocusEvent *e);

    /** support SWI... exec thread console creation */
    struct req_new_console : public QEvent {
        Swipl_IO *iop;
        QString title;
        req_new_console(Swipl_IO *iop, QString title) : QEvent(User), iop(iop), title(title) {}
    };
    virtual void customEvent(QEvent *event);

    /** handle tooltip placing */
    virtual bool event(QEvent *event);

    /** sense word under cursor for tooltip display */
    virtual bool eventFilter(QObject *, QEvent *event);

    /** output/input text attributes */
    QTextCharFormat output_text_fmt, input_text_fmt;

    /** start point of engine output insertion */
    /** i.e. keep last user editable position */
    int fixedPosition;

    /** commands to be dispatched to engine thread */
    QStringList commands;

    /** poor man command history */
    QStringList history;
    int history_next;
    QString history_spare;

    /** count output before setting cursor at end */
    int count_output;

    /** interval on count_output, determine how often to force output flushing */
    int update_refresh_rate;

    /** autocompletion - today not context sensitive */
    /** will eventually become with help from the kernel */
    typedef QCompleter t_Completion;
    t_Completion *preds;
    QStringList lmodules;

    /** factorize code, attempt to get visual clue from QCompleter */
    void compinit(QTextCursor c);
    void compinit2(QTextCursor c);

    /** associated thread id */
    int thid;

    /** wiring etc... */
    void setup();
    void setup(Swipl_IO *iop);

    /** tooltips & completion support, from helpidx.pl */
    QString last_word, last_tip;
    void set_cursor_tip(QTextCursor c);

    /** attempt to track *where* to place outpout */
    enum e_status { idle, attaching, wait_input, running, closing };
    e_status status;
    int promptPosition;
    bool is_tty;

public slots:

    /** display different cursor where editing available */
    void onCursorPositionChanged();

    /** serve console menus */
    void onConsoleMenuAction();

    /** 2. attempt to run generic code inter threads */
    void run_function(pfunc f) { f(); }

protected slots:

    /** send text to output */
    void user_output(QString text);

    /** issue an input request */
    void user_prompt(int threadId, bool tty);

    /** push command from queue to Prolog processor */
    void command_do();

    /** push completion request in current command line */
    void insertCompletion(QString);

    /** win_ ... API threaded construction: complete interactor setup */
    void attached();

    /** when engine gracefully complete-... */
    void eng_completed();

signals:

    /** issued to serve prompt */
    void user_input(QString);

    /** 3. attempt to run generic code inter threads */
    void sig_run_function(pfunc f);
};

#endif
