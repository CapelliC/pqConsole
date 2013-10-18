#ifndef CALLABLE_H
#define CALLABLE_H

#include <QObject>
#include <SWI-cpp.h>
#include "pqConsole_global.h"

/** callable abstraction */
struct PQCONSOLESHARED_EXPORT callable : QObject {
    Q_OBJECT
public:
    explicit callable(PlTermv args, bool trace = 0, QObject *p = 0);
    operator bool() const { return ok; }
    typedef PlTerm Arg;
    typedef PlTermv Args;
protected:
    Args args;
    bool trace;
    bool ok;
};

typedef callable::Arg Arg;

#define CallableA(P) \
Q_OBJECT \
public:  P(Arg a, bool trace = 0, QObject *p = 0) : callable(Args(a), trace, p) {}

#define CallableAB(P) \
Q_OBJECT \
public:  P(Arg a, Arg b, bool trace = 0, QObject *p = 0) : callable(Args(a, b), trace, p) {}

#define CallableABC(P) \
Q_OBJECT \
public:  P(Arg a, Arg b, Arg c, bool trace = 0, QObject *p = 0) : callable(Args(a, b, c), trace, p) {}

#define CallableABCD(P) \
Q_OBJECT \
public:  P(Arg a, Arg b, Arg c, Arg d, bool trace = 0, QObject *p = 0) : callable(Args(a, b, c, d), trace, p) {}

#endif // CALLABLE_H
