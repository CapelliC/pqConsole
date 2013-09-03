#ifndef PQAPPLICATION_H
#define PQAPPLICATION_H

#include <QApplication>
#include "pqMainWindow.h"

class PQCONSOLESHARED_EXPORT pqApplication : public QApplication
{
    Q_OBJECT

public:
    pqApplication(int& argc, char **argv);

protected:

    /** handle the QFileOpenEvent event */
    virtual bool event(QEvent *);

    pqMainWindow *w;
};

#endif // PQAPPLICATION_H
