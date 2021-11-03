#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
#ifdef __APPLE__
    a.setStyle(QStyleFactory::create("Fusion"));
#endif
    
    
    MainWindow w;
    w.show();

    return a.exec();
}
