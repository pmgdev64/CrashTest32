// main.cpp
#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // T?o splash screen
    QPixmap pixmap(400, 200);
    pixmap.fill(QColor(0, 0, 0));
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    painter.setPen(Qt::white);
    QFont font("Arial", 20, QFont::Bold);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "CRASH TEST");
    
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(pixmap.rect().adjusted(0, 40, 0, 0), Qt::AlignCenter, "Loading...");
    
    painter.end();
    
    QSplashScreen splash(pixmap);
    splash.show();
    app.processEvents();
    
    // *** QUAN TR?NG: Khai báo MainWindow TRÝ?C khi dùng ***
    MainWindow w;
    
    // Ð?i 1.5 giây r?i ðóng splash và show main window
    QTimer::singleShot(1500, &splash, SLOT(close()));
    QTimer::singleShot(1500, &w, SLOT(show()));
    
    return app.exec();
}
