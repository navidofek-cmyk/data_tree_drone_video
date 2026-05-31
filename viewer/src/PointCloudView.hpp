#pragma once
#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QString>
#include <QRectF>
#include <vector>
#include <array>
#include <cstdint>

// Jeden bod mračna: poloha (float, vycentrovaná na střed scény) + barva.
struct ClPoint { float x, y, z; uint8_t r, g, b; };

// Softwarový prohlížeč mračna bodů (QPainter + vlastní z-buffer, bez OpenGL).
// Z je svislá osa (výška). Tažením myší otáčíš, pravým tažením posouváš,
// kolečkem přibližuješ. Funguje i přes vzdálené X11 (ssh -X / MobaXterm).
class PointCloudView : public QWidget {
    Q_OBJECT
public:
    explicit PointCloudView(QWidget* parent = nullptr);

    // Načte binární PLY (little-endian: float x,y,z + uchar red,green,blue).
    // maxPoints > 0 ⇒ rovnoměrné podvzorkování (kvůli plynulosti na dálku).
    bool load(const QString& plyPath, int maxPoints = 0);

    // Vykreslí jeden snímek do zadaného obrázku (sdílí logiku s paintEvent).
    void renderInto(QImage& img) const;

    // Uloží aktuální pohled do PNG (vrátí cestu, prázdné = chyba).
    QString saveSnapshot();

    // Řezací box kolmý na osy: pro osu (0=X,1=Y,2=Z) zlomky [0..1] bboxu.
    void setClip(int axis, float lo, float hi);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    QSize sizeHint() const override { return {1000, 700}; }

private:
    // Rozložení navigačního gizma (osové kuličky + tlačítka) pro vykreslení i klikání.
    struct Gizmo {
        QPointF c; double r;
        std::array<QPointF,6> ax;     // +X,-X,+Y,-Y,+Z,-Z
        std::array<QRectF,6>  btn;    // zoom+, zoom-, reset, ortho/persp, mřížka, PNG
    };
    Gizmo gizmo(int w, int h) const;
    void  snapToAxis(int i);          // skok pohledu na osu

private:
    std::vector<ClPoint> pts_;
    float cx_ = 0, cy_ = 0, cz_ = 0;   // střed scény (odečítá se při projekci)
    float radius_ = 1.0f;              // poloměr bboxu (pro měřítko)
    float hx_ = 1, hy_ = 1, zmin_ = 0; // půl-rozsahy X/Y a spodek (pro mřížku)
    float bmin_[3] = {0,0,0}, bmax_[3] = {1,1,1};   // bbox po osách
    float clipLo_[3] = {0,0,0}, clipHi_[3] = {1,1,1}; // řezací box (zlomky bboxu)

    double yaw_ = 0.6, pitch_ = 0.35;  // úhly pohledu (rad), Z nahoru
    double zoom_ = 1.0;
    double panX_ = 0, panY_ = 0;       // posun v pixelech
    int    splat_ = 2;                 // velikost bodu (px)
    bool   showGrid_ = true;           // referenční mřížka na zemi
    bool   ortho_ = false;             // ortografická projekce (jinak perspektiva)

    QPointF lastPos_;
    bool dragging_ = false;            // levé = rotace
    bool panning_  = false;            // pravé  = posun
};
