#include "PointCloudView.hpp"
#include "MainWindow.hpp"
#include <QApplication>
#include <QImage>
#include <QString>
#include <cstdio>

// Vstupní bod programu.
// Použití:
//   viewer [cesta.ply] [--max N] [--snapshot out.png [W H]]
//
//   --max N        podvzorkuje na ~N bodů (plynulost přes vzdálené X11)
//   --snapshot ... vykreslí jeden snímek do PNG a skončí (test bez displeje:
//                  QT_QPA_PLATFORM=offscreen viewer cloud.ply --snapshot x.png)
int main(int argc, char** argv) {
    QApplication app(argc, argv);   // inicializace Qt (musí být první)

    // ── výchozí hodnoty + jednoduché parsování argumentů ────────────────────
    QString path = "cloud.ply";
    QString snap;                   // neprázdné = režim snímku do PNG
    int maxp = 0, sw = 1000, sh = 700;

    for (int i = 1; i < argc; ++i) {
        QString a = argv[i];
        if (a == "--max" && i+1 < argc)       maxp = QString(argv[++i]).toInt();
        else if (a == "--snapshot" && i+1 < argc) {
            snap = argv[++i];
            // volitelná šířka/výška za názvem souboru
            if (i+2 < argc && QString(argv[i+1]).toInt() > 0) {
                sw = QString(argv[++i]).toInt();
                sh = QString(argv[++i]).toInt();
            }
        } else if (!a.startsWith("--")) path = a;   // poziční argument = cesta k PLY
    }

    // ── headless režim: vyrenderuj PNG bez okna a skonči (netřeba event loop) ─
    if (!snap.isEmpty()) {
        PointCloudView view;
        if (!view.load(path, maxp)) {
            std::fprintf(stderr, "Nelze načíst PLY: %s\n", path.toUtf8().constData());
            return 1;
        }
        QImage img(sw, sh, QImage::Format_RGB32);
        view.renderInto(img);
        if (!img.save(snap)) { std::fprintf(stderr, "Nelze uložit %s\n", snap.toUtf8().constData()); return 2; }
        std::printf("Snímek uložen: %s (%dx%d)\n", snap.toUtf8().constData(), sw, sh);
        return 0;
    }

    // ── interaktivní režim: okno s 3D pohledem + panelem řezacích rovin ──────
    MainWindow win;
    if (!win.load(path, maxp)) {
        std::fprintf(stderr, "Nelze načíst PLY: %s\n", path.toUtf8().constData());
        return 1;
    }
    win.setWindowTitle("Point Cloud Viewer — " + path);
    win.resize(1220, 760);
    win.show();
    return app.exec();   // spusť smyčku událostí (běží, dokud se okno nezavře)
}
