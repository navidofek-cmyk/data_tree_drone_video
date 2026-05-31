#include "MainWindow.hpp"
#include "PointCloudView.hpp"
#include <QHBoxLayout>   // vodorovné rozložení (pohled | panel)
#include <QVBoxLayout>   // svislé rozložení (prvky panelu pod sebou)
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    // 'this' jako rodič → o uvolnění paměti se postará Qt strom (žádné delete).
    view_ = new PointCloudView(this);

    auto* panel = new QWidget(this);
    panel->setFixedWidth(210);
    auto* pv = new QVBoxLayout(panel);   // prvky panelu pod sebou

    auto* box = new QGroupBox("Řezací roviny (kolmé na osy)");
    auto* bl  = new QVBoxLayout(box);
    const char* names[3] = {"X (východ)", "Y (sever)", "Z (výška)"};

    // Pro každou osu vytvoř dvojici posuvníků min/max (rozsah 0..1000 = zlomek bboxu ×1000).
    for (int a = 0; a < 3; ++a) {
        bl->addWidget(new QLabel(names[a]));
        auto* lo = new QSlider(Qt::Horizontal); lo->setRange(0,1000); lo->setValue(0);
        auto* hi = new QSlider(Qt::Horizontal); hi->setRange(0,1000); hi->setValue(1000);
        bl->addWidget(new QLabel("min")); bl->addWidget(lo);
        bl->addWidget(new QLabel("max")); bl->addWidget(hi);
        lo_.push_back(lo); hi_.push_back(hi);   // ulož pro pozdější reset
        // SIGNÁL→SLOT: když posuvník změní hodnotu, zavolej naši lambdu (přepočet ořezu osy a).
        // [this,a] = lambda zachytí ukazatel this a kopii indexu osy a.
        connect(lo, &QSlider::valueChanged, this, [this,a]{ applyClip(a); });
        connect(hi, &QSlider::valueChanged, this, [this,a]{ applyClip(a); });
    }
    pv->addWidget(box);

    auto* reset = new QPushButton("Zrušit řez");
    connect(reset, &QPushButton::clicked, this, [this]{   // vrať všechny posuvníky na plný rozsah
        for (auto* s : lo_) s->setValue(0);
        for (auto* s : hi_) s->setValue(1000);
    });
    pv->addWidget(reset);
    pv->addStretch();   // pružina dole → prvky zůstanou nahoře

    // Celkové rozložení okna: pohled (roztažitelný) vlevo, panel (pevný) vpravo.
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0,0,0,0);
    h->addWidget(view_, 1);   // faktor 1 = bere zbývající místo
    h->addWidget(panel);
}

// Přepočte posuvníky dané osy na zlomky [0..1] a pošle je do pohledu.
void MainWindow::applyClip(int a) {
    // hlídej, ať se min nedostane nad max (a naopak) — jinak by řez „zmizel"
    if (lo_[a]->value() > hi_[a]->value()) {
        // přitlač ten druhý posuvník; sender() = který posuvník signál vyslal
        if (sender() == lo_[a]) hi_[a]->setValue(lo_[a]->value());
        else                    lo_[a]->setValue(hi_[a]->value());
        return; // setValue znovu vyvolá applyClip s opravenými hodnotami
    }
    view_->setClip(a, lo_[a]->value()/1000.0f, hi_[a]->value()/1000.0f);
}

bool MainWindow::load(const QString& plyPath, int maxPoints) {
    return view_->load(plyPath, maxPoints);
}
