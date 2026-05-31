#include "MainWindow.hpp"
#include "PointCloudView.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    view_ = new PointCloudView(this);

    auto* panel = new QWidget(this);
    panel->setFixedWidth(210);
    auto* pv = new QVBoxLayout(panel);

    auto* box = new QGroupBox("Řezací roviny (kolmé na osy)");
    auto* bl  = new QVBoxLayout(box);
    const char* names[3] = {"X (východ)", "Y (sever)", "Z (výška)"};

    for (int a = 0; a < 3; ++a) {
        bl->addWidget(new QLabel(names[a]));
        auto* lo = new QSlider(Qt::Horizontal); lo->setRange(0,1000); lo->setValue(0);
        auto* hi = new QSlider(Qt::Horizontal); hi->setRange(0,1000); hi->setValue(1000);
        bl->addWidget(new QLabel("min")); bl->addWidget(lo);
        bl->addWidget(new QLabel("max")); bl->addWidget(hi);
        lo_.push_back(lo); hi_.push_back(hi);
        connect(lo, &QSlider::valueChanged, this, [this,a]{ applyClip(a); });
        connect(hi, &QSlider::valueChanged, this, [this,a]{ applyClip(a); });
    }
    pv->addWidget(box);

    auto* reset = new QPushButton("Zrušit řez");
    connect(reset, &QPushButton::clicked, this, [this]{
        for (auto* s : lo_) s->setValue(0);
        for (auto* s : hi_) s->setValue(1000);
    });
    pv->addWidget(reset);
    pv->addStretch();

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0,0,0,0);
    h->addWidget(view_, 1);
    h->addWidget(panel);
}

// Hlídá min ≤ max a pošle zlomky do pohledu.
void MainWindow::applyClip(int a) {
    if (lo_[a]->value() > hi_[a]->value()) {
        // přitlač druhý posuvník, ať se nepřekříží
        if (sender() == lo_[a]) hi_[a]->setValue(lo_[a]->value());
        else                    lo_[a]->setValue(hi_[a]->value());
        return; // setValue znovu spustí applyClip
    }
    view_->setClip(a, lo_[a]->value()/1000.0f, hi_[a]->value()/1000.0f);
}

bool MainWindow::load(const QString& plyPath, int maxPoints) {
    return view_->load(plyPath, maxPoints);
}
