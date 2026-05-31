#pragma once
#include <QWidget>
#include <QString>
#include <vector>

// Dopředné deklarace (forward declarations): stačí říct „tahle třída existuje",
// nemusíme includovat celé hlavičky → rychlejší překlad a menší závislosti.
// Skutečné #include jsou až v .cpp, kde objekty opravdu používáme.
class PointCloudView;
class QSlider;

// Hlavní okno aplikace: vlevo 3D pohled (PointCloudView), vpravo panel
// s posuvníky řezacích rovin (kolmých na osy X/Y/Z).
class MainWindow : public QWidget {
    Q_OBJECT   // signály/sloty (posuvníky → ořez); vyžaduje moc/AUTOMOC
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Načte PLY do vnitřního pohledu (deleguje na PointCloudView::load).
    bool load(const QString& plyPath, int maxPoints = 0);

private:
    void applyClip(int axis);          // přepočte posuvníky osy na ořez a pošle do pohledu

    PointCloudView* view_ = nullptr;   // 3D pohled (vlastní ho toto okno přes parent–child)
    std::vector<QSlider*> lo_, hi_;    // posuvníky min/max pro osy X,Y,Z (po 3)
};
