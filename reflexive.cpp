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
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QPixmap>

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
    VP obj = 0;
    QString name = t2w(PL_A1);
    pqConsole::gui_run([&](){
       if (int id = QMetaType::type(name.toUtf8()))
           obj = QMetaType::construct(id);  // calls default constructor here
    });
    if (obj)
        return PL_A2 = obj;
    throw PlException("create_object failed");
}

static QVariant T2V(PlTerm pl)
{
    switch (pl.type()) {
    case PL_INTEGER:
        return QVariant(qlonglong(long(pl)));
    case PL_FLOAT:
        return QVariant(double(pl));
    case PL_ATOM:
        return QVariant(t2w(pl));

    case PL_TERM: {
        int ty = QMetaType::type(pl.name()), ar = pl.arity();

        switch (ty) {

        case QMetaType::QSize:
            return QSize(pl[1], pl[2]);

        case QMetaType::QSizeF:
            return QSizeF(pl[1], pl[2]);

        case QMetaType::QDate:
            return QDate(pl[1], pl[2], pl[3]);

        case QMetaType::QTime:
            return ar == 2 ? QTime(pl[1], pl[2]) :
                   ar == 3 ? QTime(pl[1], pl[2], pl[3]) :
                             QTime(pl[1], pl[2], pl[3], pl[4]);

        case QMetaType::QDateTime:
            return QDateTime(T2V(pl[1]).toDate(), T2V(pl[2]).toTime());

        case QMetaType::QUrl:   // TBD
            return QUrl(T2V(pl[1]).toString());

        // TBD QLocale();

        case QMetaType::QRect:
            if (ar == 2) {
                QVariant a[2] =  { T2V(pl[1]), T2V(pl[2]) };
                if (a[0].type() == QVariant::Point && a[1].type() == QVariant::Point)
                    return QRect(a[0].toPoint(), a[1].toPoint());
                if (a[0].type() == QVariant::Point && a[1].type() == QVariant::Size)
                    return QRect(a[0].toPoint(), a[1].toSize());
                break;
            }
            return QRect(pl[1], pl[2], pl[3], pl[4]);

        case QMetaType::QRectF:
            if (ar == 2) {
                QVariant a[2] =  { T2V(pl[1]), T2V(pl[2]) };
                if (a[0].type() == QVariant::PointF && a[1].type() == QVariant::PointF)
                    return QRectF(a[0].toPointF(), a[1].toPointF());
                if (a[0].type() == QVariant::PointF && a[1].type() == QVariant::SizeF)
                    return QRectF(a[0].toPointF(), a[1].toSizeF());
                break;
            }
            return QRectF(pl[1], pl[2], pl[3], pl[4]);

        case QMetaType::QLine:
            return ar == 2 ? QLine(T2V(pl[1]).toPoint(), T2V(pl[2]).toPoint()) :
                             QLine(pl[1], pl[2], pl[3], pl[4]);

        case QMetaType::QLineF:
            return ar == 2 ? QLineF(T2V(pl[1]).toPointF(), T2V(pl[2]).toPointF()) :
                             QLineF(pl[1], pl[2], pl[3], pl[4]);

        case QMetaType::QPoint:
            return QPoint(pl[1], pl[2]);

        case QMetaType::QPointF:
            return QPointF(pl[1], pl[2]);
            /*
        case QMetaType::QBrush:
            return QBrush();
            */
        }
    } }
    throw PlException("cannot convert PlTerm to QVariant");
}

/** pq_method(Object, Member, Args, Retv)
 *  note: pointers should be registered to safely exchange them
 */
PREDICATE(pq_method, 4) {
    QObject *obj = pq_cast<QObject>(PL_A1);
    if (obj) {
        QString Member = t2w(PL_A2);
        // iterate superClasses
        for (const QMetaObject *meta = obj->metaObject(); meta; meta = meta->superClass()) {
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

                            if (ipar == m.parameterTypes().size())
                                throw PlException("argument list count mismatch (too much arguments)");

                            // match variant type
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

                            va.append(QGenericArgument(vl.back().typeName(), vl.back().data()));
                        }

                        if (ipar < m.parameterTypes().size())
                            throw PlException("argument list count mismatch (miss arguments)");

                        if (m.parameterTypes().size() > 3)
                            throw PlException("unsupported call (max 3 arguments)");

                        // optional return value
                        QGenericReturnArgument rv;
                        int trv = QMetaType::type(m.typeName());

                        bool rc = false;
                        pqConsole::gui_run([&]() {
                            switch (m.parameterTypes().size()) {
                            case 0:
                                rc = trv ? m.invoke(obj, rv) : m.invoke(obj);
                                break;
                            case 1:
                                rc = trv ? m.invoke(obj, rv, va[0]) : m.invoke(obj, va[0]);
                                break;
                            case 2:
                                rc = trv ? m.invoke(obj, rv, va[0], va[1]) : m.invoke(obj, va[0], va[1]);
                                break;
                            case 3:
                                rc = trv ? m.invoke(obj, rv, va[0], va[1], va[2]) : m.invoke(obj, va[0], va[1], va[2]);
                                break;
                            }
                        });

                        if (rc && trv) {
                            // TBD unify return value
                        }

                        return rc;
                    }
                }
            }
        }
    }
    throw PlException("pq_method failed");
}

/** pq_property(Object, Property, Value)
 *  read/write a property by name
 */
PREDICATE(pq_property, 3) {
    QObject *obj = pq_cast<QObject>(PL_A1);
    if (obj) {
        // from actual class up to QObject
        for (const QMetaObject *meta = obj->metaObject(); meta; meta = meta->superClass()) {
            int ip = meta->indexOfProperty(t2w(PL_A2).toUtf8());
            if (ip >= 0) {
                QMetaProperty p = meta->property(ip);
                QVariant v;
                bool isvar = PL_A3.type() == PL_VARIABLE, rc = false;
                if (!isvar)
                    v = T2V(PL_A3);
                pqConsole::gui_run([&]() {
                    if (isvar) {
                        v = p.read(obj);
                        rc = true;
                    }
                    else {
                        rc = p.write(obj, v);
                    }
                });

                return rc;
            }
        }
    }
    throw PlException("pq_property failed");
}

/** unify a property of QObject:
 *  allows read/write of simple atomic values
 */
QString pqConsole::unify(const QMetaProperty& p, QObject *o, PlTerm v) {

    #define OK return QString()
    QVariant V;

    switch (v.type()) {

    case PL_VARIABLE: {

        gui_run([&]() { V = p.read(o); });

        switch (p.type()) {
        case QVariant::Bool:
            v = V.toBool() ? A("true") : A("false");
            OK;
        case QVariant::Int:
            if (p.isEnumType()) {
                Q_ASSERT(!p.isFlagType());  // TBD
                QMetaEnum e = p.enumerator();
                if (CCP key = e.valueToKey(V.toInt())) {
                    v = A(key);
                    OK;
                }
            }
            v = long(V.toInt());
            OK;
        case QVariant::UInt:
            v = long(V.toUInt());
            OK;
        case QVariant::String:
            v = A(V.toString());
            OK;
        default:
            break;
        }
    }   break;

    case PL_INTEGER:
        switch (p.type()) {
        case QVariant::Int:
        case QVariant::UInt:
            V = qint32(v);
            break;
        default:
            break;
        }
        break;

    case PL_ATOM:
        switch (p.type()) {
        case QVariant::String:
            V = t2w(v);
            break;
        case QVariant::Int:
            if (p.isEnumType()) {
                Q_ASSERT(!p.isFlagType());  // TBD
                int i = p.enumerator().keyToValue(v);
                if (i != -1)
                    V = i;
            }
        default:
            break;
        }
        break;

    case PL_FLOAT:
        switch (p.type()) {
        case QVariant::Double:
            V = double(v);
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    if (V.isValid()) {
        bool retok = false;
        gui_run([&]() { if (p.write(o, V)) retok = true; });
        if (retok)
            OK;
    }

    return o->tr("property %1: type mismatch").arg(p.name());
}

/** unify a property of QObject, seek by name:
 *  allows read/write of basic atomic values (note: enums are symbolics)
 */
QString pqConsole::unify(CCP name, QObject *o, PlTerm v) {
    int pid = o->metaObject()->indexOfProperty(name);
    if (pid >= 0)
        return unify(o->metaObject()->property(pid), o, v);
    return o->tr("property %1: not found").arg(name);
}
