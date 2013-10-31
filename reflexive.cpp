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

#define PROLOG_MODULE "pqConsole"
#include "PREDICATE.h"
#include "pqConsole.h"

#include <QStack>
#include <QDebug>
#include <QMetaObject>
#include <QMetaType>
#include <QVarLengthArray>

//! collect all registered Qt types
PREDICATE(list_objects_type, 1) {
    PlTail l(PL_A1);
    for (int t = 0; t < (1 << 20); ++t)
        if (QMetaType::isRegistered(t)) {
            const char* n = QMetaType::typeName(t);
            if (n && *n)
                l.append(n);
        }
    l.close();
    return TRUE;
}

//! create a meta-instantiable Qt object
PREDICATE(create_object, 2) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        VP obj = 0;
        QString name = t2w(PL_A1);
        ConsoleEdit::exec_sync s;
        c->exec_func([&]() {
            if (int id = QMetaType::type(name.toUtf8()))
                obj = QMetaType::construct(id);
            s.go();
        });
        s.stop();
        if (obj)
            return PL_A2 = obj;
    }
    throw PlException("create_object failed");
}

//! invoke(Object, Member, Args, Retv)
//! note: need to understand how to safely exchange pointers (registering seems not relevant)
PREDICATE(invoke, 4) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        QObject *obj = pq_cast<QObject>(PL_A1);
        if (obj) {
            const QMetaObject *meta = obj->metaObject();
            if (meta) {
                QString Member = t2w(PL_A2);
                bool rc = false;
                for (int i = 0; i < meta->methodCount(); ++i) {
                    QMetaMethod m = meta->method(i);
                    if (m.methodType() == m.Method && m.access() == m.Public) {
                        QString sig = m.signature();
                        if (sig.left(sig.indexOf('(')) == Member) {
                            QVariantList vl;
                            QList<QGenericArgument> va;

                            // scan the argument list
                            L Args(PL_A3); T Arg;
                            int ipar = 0;
                            for ( ; Args.next(Arg); ++ipar) {
                                qDebug() << t2w(Arg) << Arg.type();

                                if (ipar == m.parameterTypes().size())
                                    throw PlException("argument list count mismatch (too much arguments");
                                int vt = QMetaType::type(m.parameterTypes()[ipar]);

                                switch (Arg.type()) {
                                case PL_ATOM:
                                    switch (vt) {
                                    case QVariant::String:
                                        vl.append(t2w(Arg));
                                        break;
                                    default:
                                        throw PlException("invalid type");
                                    }
                                    break;
                                case PL_INTEGER:
                                    if (vt == QVariant::Int) {
                                        qlonglong l = long(Arg);
                                        vl.append(l);
                                    } else if (vt == QMetaType::VoidStar ||
                                               vt == QMetaType::QObjectStar ||
                                               vt == QMetaType::QWidgetStar ||
                                               vt >= QMetaType::User) {
                                        VP p = Arg;
                                        vl.append(QVariant(vt, &p));
                                    }
                                    else
                                        throw PlException("invalid type");
                                    break;
                                case PL_FLOAT:
                                    if (vt == QVariant::Double)
                                        vl.append(double(Arg));
                                    else
                                        throw PlException("invalid type");
                                    break;
                                default:
                                    throw PlException("unknown type conversion");
                                }

                                //va.append(QGenericArgument(m.parameterTypes()[ipar], vl.back().data()));
                                va.append(QGenericArgument(vl.back().typeName(), vl.back().data()));
                            }
                            if (ipar < m.parameterTypes().size())
                                throw PlException("argument list count mismatch (miss arguments");

                            QGenericReturnArgument rv;
                            int trv = QMetaType::type(m.typeName());

                            ConsoleEdit::exec_sync s;
                            c->exec_func([&]() {
                                switch (m.parameterTypes().size()) {
                                case 0:
                                case 1:
                                case 2:
                                    break;
                                case 3:
                                    if (trv)
                                        rc = m.invoke(obj, rv, va[0], va[1], va[2]);
                                    else
                                        rc = m.invoke(obj, va[0], va[1], va[2]);
                                    break;
                                }
                                s.go();
                            });
                            s.stop();
                            return rc;
                        }
                    }
                }
            }
        }
    }
    throw PlException("invoke failed");
}
