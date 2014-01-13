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

#include "pqMiniSyntax.h"
#include "do_events.h"
#include <QTextDocument>
#include <QDebug>
#include <QTime>

/** this is a very limited approach to highlighting Prolog syntax.
  * Just a quick alternative to properly interfacing SWI-Prolog goodies,
  * that proved to be a much harder task than I foreseen
  */
void pqMiniSyntax::setup() {
    QString number("\\d+(?:\\.\\d+)?");
    QString symbol("[a-z][A-Za-z0-9_]*");
    QString var("[A-Z_][A-Za-z0-9_]*");
    QString quoted("\"[^\"]*\"");
    QString atomq("'[^\'']*'");
    QString atombackq("`[^`]*`");
    QString charcode("0'.|0'\\t|0'\\n|0'\\r|0'\\u[0-9][0-9][0-9][0-9]");
    QString oper("[\\+\\-\\*\\/\\=\\^<>~:\\.,;\\?@#$\\\\&{}`]+");

    tokens = QRegExp(QString("(%1)|(%2)|(%3)|(%4)|(%5)|(%6)|(%7)|(%8)|%").arg(number, symbol, var, quoted, atomq, atombackq, charcode, oper));

    fmt[Comment].setForeground(Qt::darkGreen);
    fmt[Number].setForeground(QColor("blueviolet"));
    fmt[Atom].setForeground(Qt::blue);
    fmt[Atomq].setForeground(Qt::blue);
    fmt[Atombackq].setForeground(Qt::darkYellow);
    fmt[String].setForeground(Qt::magenta);
    fmt[Variable].setForeground(QColor("brown"));
    fmt[Operator].setFontWeight(QFont::Bold);
    fmt[CharCode].setForeground(Qt::darkCyan);
    fmt[Unknown].setForeground(Qt::darkRed);
}

/** handle nested comments and simple minded Prolog syntax
  */
void pqMiniSyntax::highlightBlock(const QString &text)
{
    // simple state machine
    int nb = currentBlock().blockNumber();
    if (nb > 0) {
        setCurrentBlockState(previousBlockState());
        if (nb % 10 == 0)
            do_events();
    }
    else {
        startToEnd.start();
        qDebug() << "starting at " << QTime::currentTime();
        setCurrentBlockState(0);
    }

    for (int i = 0, j, l; ; i = j + l)
        if (currentBlockState()) {  // in multiline comment
            if ((j = text.indexOf("*/", i)) == -1) {
                setFormat(i, text.length() - i, fmt[Comment]);
                break;
            }
            setFormat(i, j - i + (l = 2), fmt[Comment]);
            setCurrentBlockState(0);
        } else {
            if ((j = tokens.indexIn(text, i)) == -1)
                break;
            QStringList ml = tokens.capturedTexts();
            Q_ASSERT(ml.length() == 8+1);
            if ((l = ml[1].length())) {  // number
                setFormat(j, l, fmt[Number]); } else
            if ((l = ml[2].length())) {  // symbol
                setFormat(j, l, fmt[Atom]); } else
            if ((l = ml[3].length())) {  // var
                setFormat(j, l, fmt[Variable]); } else
            if ((l = ml[4].length())) {  // double quoted string
                setFormat(j, l, fmt[String]); } else
            if ((l = ml[5].length())) {  // quoted atom
                setFormat(j, l, fmt[Atomq]); } else
            if ((l = ml[6].length())) {  // quoted atom
                setFormat(j, l, fmt[Atombackq]); } else
            if ((l = ml[7].length())) {  // char code
                setFormat(j, l, fmt[CharCode]); } else
            if ((l = ml[8].length())) {  // operator
                // or comment ?
                if (l >= 2 && ml[8].left(2) == "/*") {
                    setFormat(j, l = 2, fmt[Comment]);
                    setCurrentBlockState(1);
                } else
                    setFormat(j, l, fmt[Operator]); } else
            {   // single line comment
                setFormat(j, text.length() - i, fmt[Comment]);
                break;
            }
        }

    if (currentBlock() == document()->lastBlock())
        qDebug() << "done at " << QTime::currentTime() << "in" << startToEnd.elapsed();
}
