#pragma once
#include <QWidget>
#include <QString>
#include <vector>

class PointCloudView;
class QSlider;

// Hlavní okno: vlevo 3D pohled, vpravo panel s řezacími rovinami (kolmými na osy).
class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool load(const QString& plyPath, int maxPoints = 0);

private:
    void applyClip(int axis);

    PointCloudView* view_ = nullptr;
    std::vector<QSlider*> lo_, hi_;   // posuvníky min/max pro osy X,Y,Z
};
