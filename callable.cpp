#include "callable.h"
#include "PREDICATE.h"
#include <QDebug>

callable::callable(PlTermv args, bool trace, QObject *p) : QObject(p), args(args), trace(trace), ok() {
    try {
        ok = PlCall(metaObject()->className(), args) == TRUE;
        if (trace)
            qDebug() << ok << metaObject()->className();
    }
    catch (PlException e) { qDebug() << t2w(e); }
}
