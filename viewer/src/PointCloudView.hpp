#pragma once
// #pragma once = vlož tuhle hlavičku do překladu jen jednou (ochrana proti
// vícenásobnému includu). Alternativa ke klasickým #ifndef/#define strážím.

#include <QWidget>    // základní třída okenního prvku (od ní dědíme)
#include <QImage>     // rastrový obrázek v paměti = náš "framebuffer"
#include <QPointF>    // 2D bod (double)
#include <QString>
#include <QRectF>     // 2D obdélník (double) — obdélníky tlačítek gizma
#include <vector>     // std::vector — dynamické pole bodů
#include <array>      // std::array — pole pevné velikosti (osy/tlačítka gizma)
#include <cstdint>    // uint8_t / uint32_t — typy s PŘESNOU velikostí (8/32 bitů)

// Jeden bod mračna: poloha (float) + barva (3× 8bit). Pořadí členů drží paměť
// kompaktní (3×4 B + 3×1 B = 15 B, zarovnáno typicky na 16 B). uint8_t = 0..255.
struct ClPoint { float x, y, z; uint8_t r, g, b; };

// Softwarový prohlížeč mračna bodů: 3D promítáme a kreslíme SAMI (QPainter +
// vlastní z-buffer), BEZ OpenGL — aby to fungovalo i přes vzdálené X11
// (ssh -X / MobaXterm), kde moderní OpenGL přes indirect-GLX selhává.
// Konvence: osa Z = výška (nahoru). Ovládání ve stylu Blenderu.
class PointCloudView : public QWidget {   // "public QWidget" = dědíme chování okna
    Q_OBJECT   // makro Qt: zapíná meta-objektový systém (signály/sloty, introspekci).
               // Vyžaduje průchod nástroje 'moc' — v CMake zařizuje AUTOMOC.
public:
    // explicit = zákaz implicitní konverze (QWidget* se "nepřetypuje" na View).
    explicit PointCloudView(QWidget* parent = nullptr);

    // Načte binární PLY (little-endian: float x,y,z + uchar red,green,blue).
    // maxPoints > 0 ⇒ rovnoměrné podvzorkování (plynulost přes pomalé X11).
    // Vrací false při chybě (soubor/formát).
    bool load(const QString& plyPath, int maxPoints = 0);

    // Vykreslí jeden snímek do zadaného obrázku. const = nemění stav objektu.
    // Sdílí ji paintEvent (na obrazovku) i saveSnapshot (do PNG, headless).
    void renderInto(QImage& img) const;

    // Uloží aktuální pohled do PNG (vrátí cestu k souboru; prázdný řetězec = chyba).
    QString saveSnapshot();

    // Řezací rovina kolmá na osu: pro osu (0=X,1=Y,2=Z) nastaví meze jako
    // zlomky [0..1] bounding boxu. Body mimo se nevykreslí.
    void setClip(int axis, float lo, float hi);

protected:
    // Přepsané (override) virtuální metody QWidgetu. Qt je volá ve správný čas —
    // "framework volá tebe". 'override' nechá kompilátor zkontrolovat, že opravdu
    // přepisuju existující virtuální metodu (chrání před překlepem v signatuře).
    void paintEvent(QPaintEvent*) override;        // překreslení okna
    void mousePressEvent(QMouseEvent*) override;   // stisk tlačítka myši
    void mouseReleaseEvent(QMouseEvent*) override; // uvolnění (ukončí tažení)
    void mouseMoveEvent(QMouseEvent*) override;    // pohyb myši (jen při tažení)
    void wheelEvent(QWheelEvent*) override;        // kolečko = zoom
    void keyPressEvent(QKeyEvent*) override;       // klávesnice
    QSize sizeHint() const override { return {1000, 700}; }  // doporučená velikost okna

private:
    // ── navigační gizmo (Blender-styl) ─────────────────────────────────────
    // Rozložení gizma pro DANOU velikost okna — sdílí ho vykreslení i klikání,
    // aby seděly pozice. Drží střed, poloměr, 6 konců os a 6 obdélníků tlačítek.
    struct Gizmo {
        QPointF c; double r;
        std::array<QPointF,6> ax;     // konce os: +X,-X,+Y,-Y,+Z,-Z (v pixelech)
        std::array<QRectF,6>  btn;    // tlačítka: zoom+, zoom-, reset, ortho/persp, mřížka, PNG
    };
    Gizmo gizmo(int w, int h) const;  // spočítá rozložení gizma
    void  snapToAxis(int i);          // skok pohledu kolmo na osu i (+ přepne na ortho)

private:
    // ── data mračna ────────────────────────────────────────────────────────
    std::vector<ClPoint> pts_;        // všechny body (na haldě; ~16 B/bod)

    // ── geometrie scény (spočítá load z bounding boxu) ──────────────────────
    float cx_ = 0, cy_ = 0, cz_ = 0;  // střed scény (odečítá se před projekcí → rotace kolem středu)
    float radius_ = 1.0f;             // poloměr bboxu → základ měřítka
    float hx_ = 1, hy_ = 1, zmin_ = 0;// půl-rozsahy X/Y a spodek (pro mřížku na zemi)
    float bmin_[3] = {0,0,0}, bmax_[3] = {1,1,1};     // bbox po osách (pro řez)
    float clipLo_[3] = {0,0,0}, clipHi_[3] = {1,1,1}; // řezací box jako zlomky bboxu (0..1)

    // ── stav kamery/pohledu (mění ho myš a klávesy) ─────────────────────────
    double yaw_ = 0.6, pitch_ = 0.35; // úhly otočení (radiány); Z je nahoru
    double zoom_ = 1.0;               // přiblížení
    double panX_ = 0, panY_ = 0;      // posun pohledu v pixelech
    int    splat_ = 2;                // velikost bodu na obrazovce (px)
    bool   showGrid_ = true;          // kreslit referenční mřížku na zemi?
    bool   ortho_ = false;            // true = ortografie (kolmé pohledy), false = perspektiva

    // ── stav tažení myší ────────────────────────────────────────────────────
    QPointF lastPos_;                 // poslední pozice myši (na výpočet rozdílu)
    bool dragging_ = false;           // levé/prostřední tažení = rotace
    bool panning_  = false;           // pravé/Shift+prostřední = posun
};
