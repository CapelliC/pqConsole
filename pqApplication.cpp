#include "pqApplication.h"
#include "SwiPrologEngine.h"
#include "PREDICATE.h"
#include <QDebug>

pqApplication::pqApplication(int& argc, char **argv) :
    QApplication(argc, argv)
{
    // I've moved the window locally, to reference in event handler
    w = new pqMainWindow(argc, argv);
    w->show();
}

bool pqApplication::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::FileOpen:
    {   QString name = static_cast<QFileOpenEvent *>(event)->file();
        qDebug() << "FileOpen: " << name;
        SwiPrologEngine::in_thread _it;
        try {
            PlCall("prolog", "file_open_event", PlTermv(name.toStdWString().data()));
        } catch(PlException e) {
            qDebug() << CCP(e);
        }
        return true;
    }
    default:
        return QApplication::event(event);
    }
}
