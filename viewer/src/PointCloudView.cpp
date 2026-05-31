#include "PointCloudView.hpp"
#include <QPainter>      // 2D kreslení do QImage/widgetu
#include <QMouseEvent>   // události myši
#include <QWheelEvent>   // kolečko
#include <QKeyEvent>     // klávesnice
#include <QLineF>        // vzdálenost bod–bod (hit-test gizma)
#include <QDateTime>     // časové razítko do názvu PNG
#include <QDir>          // absolutní cesta k uloženému PNG
#include <fstream>       // čtení PLY (ifstream)
#include <sstream>       // parsování řádků hlavičky
#include <string>
#include <cstring>       // std::memcpy
#include <cmath>         // sin/cos/pow, M_PI
#include <algorithm>     // std::min/max/clamp/sort/swap
#include <limits>        // numeric_limits (inicializace z-bufferu)

// Konstruktor: nastav minimální velikost a ať widget může chytat klávesy.
PointCloudView::PointCloudView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);  // bez focusu by widget nedostával keyPressEvent
}

// ── Načtení binárního PLY (little-endian) ───────────────────────────────
// Robustní vůči pořadí/typu vlastností: z hlavičky spočítá offsety x,y,z
// (float/double) a red,green,blue (uchar) a pak čte záznam po záznamu.
bool PointCloudView::load(const QString& plyPath, int maxPoints) {
    std::ifstream f(plyPath.toStdString(), std::ios::binary);  // binární režim
    if (!f) return false;                                      // soubor nejde otevřít

    std::string line;
    std::getline(f, line);
    if (line.rfind("ply", 0) != 0) return false;   // rfind(…,0)==0 ⇒ řetězec ZAČÍNÁ na "ply"

    bool binLE = false;       // je to binary_little_endian?
    size_t vcount = 0;        // počet bodů (z "element vertex N")
    // Field = jedna vlastnost bodu v hlavičce: jméno, velikost v bajtech, offset v záznamu, typ.
    struct Field { std::string name; int size; int offset; std::string type; };
    std::vector<Field> fields;
    int rec = 0;              // celková velikost jednoho záznamu (bodu) v bajtech

    // Lambda: kolik bajtů zabírá daný PLY typ. (Lambda = malá funkce na místě.)
    auto typeSize = [](const std::string& t) -> int {
        if (t=="char"||t=="uchar"||t=="int8"||t=="uint8")   return 1;
        if (t=="short"||t=="ushort"||t=="int16"||t=="uint16") return 2;
        if (t=="int"||t=="uint"||t=="int32"||t=="uint32"||t=="float"||t=="float32") return 4;
        if (t=="double"||t=="float64"||t=="int64"||t=="uint64") return 8;
        return 0;
    };

    // Čti hlavičku řádek po řádku, dokud nenarazíš na "end_header".
    while (std::getline(f, line)) {
        if (line.rfind("format", 0) == 0) {
            binLE = line.find("binary_little_endian") != std::string::npos;
        } else if (line.rfind("element vertex", 0) == 0) {
            vcount = std::stoul(line.substr(std::string("element vertex").size()));
        } else if (line.rfind("property", 0) == 0) {
            // "property <typ> <jméno>" → zapiš pole + posuň offset o jeho velikost
            std::istringstream is(line);
            std::string p, t, n; is >> p >> t >> n;
            int sz = typeSize(t);
            fields.push_back({n, sz, rec, t});
            rec += sz;
        } else if (line.rfind("end_header", 0) == 0) {
            break;   // dál už jsou binární data
        }
    }
    if (!binLE || vcount == 0 || rec == 0) return false;  // nepodporovaný/prázdný PLY

    // Najdi offsety vlastností, které nás zajímají (-1 = chybí).
    int ox=-1, oy=-1, oz=-1, orr=-1, og=-1, ob=-1;
    std::string tx, ty, tz;
    for (auto& fl : fields) {
        if (fl.name=="x") { ox=fl.offset; tx=fl.type; }
        else if (fl.name=="y") { oy=fl.offset; ty=fl.type; }
        else if (fl.name=="z") { oz=fl.offset; tz=fl.type; }
        else if (fl.name=="red")   orr=fl.offset;
        else if (fl.name=="green") og=fl.offset;
        else if (fl.name=="blue")  ob=fl.offset;
    }
    if (ox<0||oy<0||oz<0) return false;   // bez souřadnic nemá smysl pokračovat

    // Přečte souřadnici z bajtů na offsetu. memcpy = bezpečné čtení bez problémů
    // se zarovnáním/aliasingem (nečteme přes přetypovaný ukazatel).
    auto readCoord = [](const char* b, int off, const std::string& t) -> float {
        if (t=="double"||t=="float64") { double d; std::memcpy(&d, b+off, 8); return float(d); }
        float v; std::memcpy(&v, b+off, 4); return v;
    };

    // Podvzorkování: ber jen každý 'stride'-tý bod, ať se vejdeme do ~maxPoints.
    const size_t stride = (maxPoints>0 && vcount>(size_t)maxPoints)
                              ? vcount/(size_t)maxPoints : 1;
    pts_.clear();
    pts_.reserve(maxPoints>0 ? (size_t)maxPoints+1 : vcount);  // předalokuj (míň realokací)

    std::vector<char> buf(rec);          // dočasný buffer na jeden záznam
    for (size_t i=0; i<vcount; ++i) {
        f.read(buf.data(), rec);         // načti celý záznam (rec bajtů)
        if (!f) break;                   // konec/chyba souboru
        if (i % stride != 0) continue;   // přeskoč kvůli podvzorkování
        ClPoint p;
        p.x = readCoord(buf.data(), ox, tx);
        p.y = readCoord(buf.data(), oy, ty);
        p.z = readCoord(buf.data(), oz, tz);
        // barva: PLY ji má jako uchar (0..255); když chybí, dej šedou (200).
        p.r = (orr>=0) ? (uint8_t)buf[orr] : 200;
        p.g = (og >=0) ? (uint8_t)buf[og]  : 200;
        p.b = (ob >=0) ? (uint8_t)buf[ob]  : 200;
        pts_.push_back(p);
    }
    if (pts_.empty()) return false;

    // bbox (krajní souřadnice) → střed a poloměr scény (na centrování a měřítko).
    float xmin=1e30f,ymin=1e30f,zmin=1e30f,xmax=-1e30f,ymax=-1e30f,zmax=-1e30f;
    for (auto& p : pts_) {
        xmin=std::min(xmin,p.x); ymin=std::min(ymin,p.y); zmin=std::min(zmin,p.z);
        xmax=std::max(xmax,p.x); ymax=std::max(ymax,p.y); zmax=std::max(zmax,p.z);
    }
    cx_=0.5f*(xmin+xmax); cy_=0.5f*(ymin+ymax); cz_=0.5f*(zmin+zmax);   // střed
    radius_ = 0.5f*std::max({xmax-xmin, ymax-ymin, zmax-zmin});         // půl nejdelší strany
    if (radius_ < 1e-3f) radius_ = 1.0f;                               // pojistka proti 0
    hx_ = 0.5f*(xmax-xmin); hy_ = 0.5f*(ymax-ymin); zmin_ = zmin;      // pro mřížku
    bmin_[0]=xmin; bmin_[1]=ymin; bmin_[2]=zmin;                       // pro řez
    bmax_[0]=xmax; bmax_[1]=ymax; bmax_[2]=zmax;
    return true;
}

// ── Vykreslení do obrázku (vlastní z-buffer) ────────────────────────────
// Tahle metoda je jádro „3D bez OpenGL": každý bod promítneme na pixel a
// uložíme jen ten nejbližší (z-buffer). Sdílí ji paintEvent i saveSnapshot.
void PointCloudView::renderInto(QImage& img) const {
    const int w = img.width(), h = img.height();
    img.fill(QColor(18, 20, 24));                 // pozadí (tmavé)
    if (pts_.empty() || w<=0 || h<=0) return;

    // sin/cos úhlů spočítej jednou (ne v každém bodě).
    const double cy = std::cos(yaw_),   sy = std::sin(yaw_);
    const double cp = std::cos(pitch_), sp = std::sin(pitch_);
    const double scaleO = zoom_ * 0.8 * std::min(w, h) / (2.0 * radius_);  // měřítko pro ortho
    const double focal  = 1.2 * std::min(w, h);                            // „ohnisko" perspektivy (pevné fov)
    const double camD   = 3.0 * radius_ / std::max(0.05, zoom_);           // vzdálenost kamery (zoom ji mění)
    const double ox = w*0.5 + panX_;                                       // střed obrazu + posun
    const double oy = h*0.5 + panY_;

    // Projekce 3D→2D. Vstup je už relativně ke středu scény. Vrací hloubku (na z-buffer).
    auto project = [&](double x, double y, double z, double& px, double& py)->double {
        // 1) otoč kolem svislé osy Z (yaw): míchá X a Y přes sin/cos
        double x1 =  x*cy - y*sy;
        double y1 =  x*sy + y*cy;
        double z1 =  z;
        // 2) sklon (pitch): co je na obrazovce vodorovně/svisle a co „do hloubky"
        double sX = x1;                    // vodorovně
        double sY = z1*cp - y1*sp;         // svisle (Z = výška jde nahoru)
        double depth = y1*cp + z1*sp;      // do obrazovky (menší = blíž ke kameře)
        if (ortho_) {                      // ortografie: bez dělení hloubkou (rovnoběžky zůstanou)
            px = ox + sX*scaleO;
            py = oy - sY*scaleO;           // mínus: obrazové y roste dolů
        } else {                           // perspektiva: dělení hloubkou → vzdálené menší
            double vd = camD + depth; if (vd < 1e-3) vd = 1e-3;  // ořez bodů za kamerou
            double f = focal / vd;
            px = ox + sX*f;
            py = oy - sY*f;
        }
        return depth;
    };

    // ── referenční mřížka na zemi (kreslí se PŘED body → vyjde „za" mračnem) ──
    if (showGrid_) {
        QPainter g(&img);
        g.setRenderHint(QPainter::Antialiasing, true);
        // vyber „hezký" krok rastru tak, aby přes scénu bylo ~12–24 dílků
        double span = 2.0 * std::max(hx_, hy_);
        const double cand[] = {0.25,0.5,1,2,5,10,20,50};
        double step = 1.0;
        for (double c : cand) { if (span/c <= 24) { step = c; break; } }
        double gz = double(zmin_) - cz_;             // rovina země (ve vycentrovaných souřadnicích)
        int nx = int(hx_/step) + 1, ny = int(hy_/step) + 1;
        g.setPen(QPen(QColor(60,70,82), 0.8));
        double px,py,qx,qy;
        // čáry rovnoběžné s Y (mění se X)
        for (int i=-nx; i<=nx; ++i) {
            project(i*step, -ny*step, gz, px,py); project(i*step, ny*step, gz, qx,qy);
            g.drawLine(QPointF(px,py), QPointF(qx,qy));
        }
        // čáry rovnoběžné s X (mění se Y)
        for (int j=-ny; j<=ny; ++j) {
            project(-nx*step, j*step, gz, px,py); project(nx*step, j*step, gz, qx,qy);
            g.drawLine(QPointF(px,py), QPointF(qx,qy));
        }
        g.setPen(QColor(110,120,135));
        project(0,0,gz,px,py);
        g.drawText(QPointF(px+4,py-4), QString("krok %1 m").arg(step,0,'g',3));  // popisek měřítka
    }

    // řezací box (kolmý na osy): přepočti zlomky [0..1] na absolutní meze v souřadnicích bodů
    float clipMin[3], clipMax[3];
    for (int a=0;a<3;a++){
        clipMin[a]=bmin_[a]+clipLo_[a]*(bmax_[a]-bmin_[a]);
        clipMax[a]=bmin_[a]+clipHi_[a]*(bmax_[a]-bmin_[a]);
    }

    // ── body přes vlastní z-buffer (přímý zápis pixelů) ───────────────────
    // z-buffer = na každý pixel nejmenší dosud viděná hloubka. Bod přepíše pixel,
    // jen když je blíž → správné překrytí bez třídění.
    std::vector<float> zbuf((size_t)w*h, std::numeric_limits<float>::max());
    const int s = std::max(1, splat_);   // bod kreslíme jako čtvereček s×s px (viditelnost)
    for (const auto& p : pts_) {          // reference (&) ⇒ bez kopírování bodů
        // ořez: zahoď body mimo řezací box
        if (p.x<clipMin[0]||p.x>clipMax[0]||p.y<clipMin[1]||p.y>clipMax[1]||
            p.z<clipMin[2]||p.z>clipMax[2]) continue;
        double px,py;
        double depth = project(p.x-cx_, p.y-cy_, p.z-cz_, px,py);  // odečti střed → projekce
        int ix = int(px), iy = int(py);
        if (ix< -s||iy< -s||ix>=w+s||iy>=h+s) continue;            // mimo obraz
        // zabal barvu do 32bit pixelu ARGB: 0xAA RR GG BB (A=255 neprůhledné)
        uint32_t col = 0xff000000u | (uint32_t(p.r)<<16) | (uint32_t(p.g)<<8) | uint32_t(p.b);
        for (int dy=0; dy<s; ++dy) {
            int yy = iy+dy; if (yy<0||yy>=h) continue;
            // scanLine(y) = ukazatel na řádek; přetypujeme bajty na 32bit pixely
            uint32_t* row = reinterpret_cast<uint32_t*>(img.scanLine(yy));
            float* zr = &zbuf[(size_t)yy*w];
            for (int dx=0; dx<s; ++dx) {
                int xx = ix+dx; if (xx<0||xx>=w) continue;
                if (depth < zr[xx]) { zr[xx]=float(depth); row[xx]=col; }  // bližší vyhrává
            }
        }
    }

    // ── HUD (text) ─────────────────────────────────────────────────────────
    QPainter g(&img);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setPen(QColor(200,205,210));
    g.drawText(QRect(8,6,w-16,18), Qt::AlignLeft,
        QString("Mračno: %1 bodů · poloměr %2 m · mřížka %3")
            .arg(pts_.size()).arg(radius_,0,'f',1).arg(showGrid_?"on":"off"));
    g.setPen(QColor(120,125,130));
    g.drawText(QRect(8,h-22,w-16,18), Qt::AlignLeft,
        "orbit: levé/prostřední tažení · posun: pravé/Shift+prostřední · kolečko=zoom · klikni osu gizma · P=PNG · šipky/WASD/+−/[ ]/g/r · 1·3·7 pohledy");

    // ── navigační gizmo (Blender-styl): osové kuličky + tlačítka ──────────
    const Gizmo G = gizmo(w,h);
    g.setPen(QPen(QColor(95,100,112),1.0));
    for(int i=0;i<6;i+=2) g.drawLine(G.c, G.ax[i]);   // čáry od středu ke kladným osám

    const QColor colp[3]={QColor(232,96,96),QColor(120,205,120),QColor(120,155,238)}; // X/Y/Z barvy
    const double V[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};        // směry os
    int order[6]={0,1,2,3,4,5};
    // seřaď osy podle hloubky, ať se přední kuličky kreslí navrch (malířův algoritmus)
    auto depthOf=[&](int i){double y1=V[i][0]*sy+V[i][1]*cy, z1=V[i][2]; return y1*cp+z1*sp;};
    std::sort(order, order+6, [&](int a,int b){return depthOf(a)>depthOf(b);}); // zadní první
    const char* AL[6]={"X","","Y","","Z",""};   // popisky jen pro kladné osy
    for(int k=0;k<6;k++){ int i=order[k]; QColor c=colp[i/2];
        if(i%2==0){ // kladná osa: plná kulička s písmenem
            g.setBrush(c); g.setPen(QPen(c.darker(140),1)); g.drawEllipse(G.ax[i],9,9);
            g.setPen(Qt::white); g.drawText(QRectF(G.ax[i].x()-9,G.ax[i].y()-9,18,18),Qt::AlignCenter,AL[i]); }
        else { // záporná osa: prázdný kroužek
            g.setBrush(QColor(30,33,40)); g.setPen(QPen(c,1.5)); g.drawEllipse(G.ax[i],6.5,6.5); } }

    // tlačítka: + − reset ortho/persp mřížka PNG (aktivní stav se zvýrazní)
    const char* BL[6]={"+","−","⌂", ortho_?"⊥":"∞", "#","PNG"};
    g.setFont(QFont(g.font().family(), 8));
    for(int i=0;i<6;i++){
        bool active = (i==3 && ortho_) || (i==4 && showGrid_);
        g.setBrush(active ? QColor(70,90,120) : QColor(42,46,54));
        g.setPen(QColor(95,100,112)); g.drawRoundedRect(G.btn[i],4,4);
        g.setPen(QColor(215,220,226)); g.drawText(G.btn[i],Qt::AlignCenter,BL[i]); }
    g.setPen(QColor(120,125,130));
    g.drawText(QRectF(G.btn[3].x()-64, G.btn[3].y(), 60, 24), Qt::AlignVCenter|Qt::AlignRight,
               ortho_?"ortho":"persp");   // popisek aktuální projekce
}

// Spočítá rozložení gizma pro dané rozměry okna. Stejné použije render i klikání,
// aby pozice seděly. const = nemění stav objektu.
PointCloudView::Gizmo PointCloudView::gizmo(int w, int h) const {
    Gizmo G; G.c = QPointF(w-60.0, 62.0); G.r = 27.0;   // střed vpravo nahoře, poloměr
    const double cy=std::cos(yaw_), sy=std::sin(yaw_), cp=std::cos(pitch_), sp=std::sin(pitch_);
    const double V[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    // promítni jednotkové osy stejnou rotací jako scénu (ale bez perspektivy) → 2D body
    for(int i=0;i<6;i++){
        double x1=V[i][0]*cy-V[i][1]*sy, y1=V[i][0]*sy+V[i][1]*cy, z1=V[i][2];
        double sX=x1, sY=z1*cp-y1*sp;
        G.ax[i]=G.c+QPointF(sX*G.r, -sY*G.r);
    }
    // tlačítka ve svislém sloupci pod gizmem
    const double sz=24.0, gap=5.0, bx=w-34.0, by0=G.c.y()+G.r+16.0;
    for(int i=0;i<6;i++) G.btn[i]=QRectF(bx, by0+i*(sz+gap), sz, sz);
    return G;
}

// Skok pohledu kolmo na osu i (a přepnutí na ortho – jako v Blenderu).
void PointCloudView::snapToAxis(int i){
    ortho_ = true;
    const double H = M_PI/2*0.999;   // skoro 90° (přesných 90° je degenerované)
    switch(i){
        case 0: yaw_= M_PI/2; pitch_=0; break;  // +X zprava
        case 1: yaw_=-M_PI/2; pitch_=0; break;  // -X zleva
        case 2: yaw_= M_PI;   pitch_=0; break;  // +Y zezadu
        case 3: yaw_= 0;      pitch_=0; break;  // -Y zepředu
        case 4: yaw_= 0;      pitch_= H; break; // +Z shora
        case 5: yaw_= 0;      pitch_=-H; break; // -Z zespodu
    }
}

// Nastaví řez pro osu (0..2). Hlídá lo≤hi a rozsah [0,1], pak překreslí.
void PointCloudView::setClip(int axis, float lo, float hi){
    if (axis<0||axis>2) return;
    if (lo>hi) std::swap(lo,hi);
    clipLo_[axis]=std::clamp(lo,0.0f,1.0f);
    clipHi_[axis]=std::clamp(hi,0.0f,1.0f);
    update();   // naplánuj překreslení
}

// Vyrenderuje aktuální pohled do PNG (název s časovým razítkem). Funguje i headless.
QString PointCloudView::saveSnapshot(){
    QImage img(std::max(640,width()), std::max(480,height()), QImage::Format_RGB32);
    renderInto(img);
    QString name = "view_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";
    if(!img.save(name)) return QString();
    return QDir::current().absoluteFilePath(name);
}

// Qt nás volá, když se má okno překreslit. Vyrenderujeme do obrázku a hodíme ho na widget.
void PointCloudView::paintEvent(QPaintEvent*) {
    QImage img(size(), QImage::Format_RGB32);
    renderInto(img);
    QPainter p(this);
    p.drawImage(0, 0, img);
}

// Stisk myši: levým klikem nejdřív zkus gizmo (osy/tlačítka), jinak začni tažení.
void PointCloudView::mousePressEvent(QMouseEvent* e) {
    lastPos_ = e->pos();
    if (e->button()==Qt::LeftButton) {
        const Gizmo G = gizmo(width(), height());
        const QPointF m = e->pos();
        // klik blízko konce osy ⇒ skok kolmo na tu osu
        for (int i=0;i<6;i++)
            if (QLineF(m, G.ax[i]).length() <= 11.0) { snapToAxis(i); update(); return; }
        // klik na tlačítko ⇒ jeho akce
        for (int i=0;i<6;i++) if (G.btn[i].contains(m)) {
            switch(i){
                case 0: zoom_=std::clamp(zoom_*1.2,0.1,50.0); break;   // zoom +
                case 1: zoom_=std::clamp(zoom_/1.2,0.1,50.0); break;   // zoom -
                case 2: yaw_=0.6; pitch_=0.35; zoom_=1.0; panX_=panY_=0; ortho_=false; break; // reset
                case 3: ortho_=!ortho_; break;                          // persp/ortho
                case 4: showGrid_=!showGrid_; break;                    // mřížka
                case 5: { QString p=saveSnapshot(); if(!p.isEmpty()) qInfo("PNG uloženo: %s", qPrintable(p)); } break;
            }
            update(); return;
        }
    }
    // jinak: urči režim tažení podle tlačítka/Shiftu (Blender-styl)
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    if (e->button()==Qt::LeftButton || (e->button()==Qt::MiddleButton && !shift)) dragging_ = true;
    else if (e->button()==Qt::RightButton || (e->button()==Qt::MiddleButton && shift)) panning_ = true;
}

// Uvolnění tlačítka ukončí tažení (jinak by mouseMove rotoval i bez stisku).
void PointCloudView::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false; panning_ = false;
}

// Pohyb myši (Qt ho posílá jen při stisknutém tlačítku): rotace nebo posun podle režimu.
void PointCloudView::mouseMoveEvent(QMouseEvent* e) {
    QPointF d = QPointF(e->pos()) - lastPos_;   // o kolik se myš posunula od minula
    lastPos_ = e->pos();
    if (dragging_) {
        yaw_   += d.x()*0.01;                   // vodorovný pohyb → yaw
        pitch_ += d.y()*0.01;                   // svislý → pitch
        pitch_  = std::clamp(pitch_, -1.55, 1.55);  // ať se pohled nepřetočí přes pól
        update();
    } else if (panning_) {
        panX_ += d.x();                         // posun pohledu (v pixelech)
        panY_ += d.y();
        update();
    }
}

// Kolečko = zoom. Exponenciální (násobení) ⇒ plynulé v obou směrech.
void PointCloudView::wheelEvent(QWheelEvent* e) {
    double f = std::pow(1.0015, e->angleDelta().y());
    zoom_ = std::clamp(zoom_*f, 0.1, 50.0);
    update();
}

// Klávesnice: rotace/posun/zoom/pohledy/přepínače. update() na konci překreslí.
void PointCloudView::keyPressEvent(QKeyEvent* e) {
    const double rot = 0.08, pan = 30.0;   // krok rotace (rad) a posunu (px)
    switch (e->key()) {
        // rotace šipkami
        case Qt::Key_Left:  yaw_   -= rot; break;
        case Qt::Key_Right: yaw_   += rot; break;
        case Qt::Key_Up:    pitch_  = std::clamp(pitch_-rot, -1.55, 1.55); break;
        case Qt::Key_Down:  pitch_  = std::clamp(pitch_+rot, -1.55, 1.55); break;
        // posun WASD
        case Qt::Key_A: panX_ += pan; break;
        case Qt::Key_D: panX_ -= pan; break;
        case Qt::Key_W: panY_ += pan; break;
        case Qt::Key_S: panY_ -= pan; break;
        // zoom +/-
        case Qt::Key_Plus: case Qt::Key_Equal: zoom_ = std::clamp(zoom_*1.15, 0.1, 50.0); break;
        case Qt::Key_Minus:                    zoom_ = std::clamp(zoom_/1.15, 0.1, 50.0); break;
        // velikost bodu [ ]
        case Qt::Key_BracketRight: splat_ = std::min(splat_+1, 6); break;
        case Qt::Key_BracketLeft:  splat_ = std::max(splat_-1, 1); break;
        // mřížka / reset
        case Qt::Key_G: showGrid_ = !showGrid_; break;
        case Qt::Key_R: yaw_=0.6; pitch_=0.35; zoom_=1.0; panX_=panY_=0; break;
        // pohledy (numpad/číslice): 1=zepředu 3=zprava 7=shora
        case Qt::Key_1: snapToAxis(3); break;
        case Qt::Key_3: snapToAxis(0); break;
        case Qt::Key_7: snapToAxis(4); break;
        case Qt::Key_5: ortho_ = !ortho_; break;   // persp/ortho (jako numpad 5)
        // export PNG
        case Qt::Key_P: { QString p=saveSnapshot(); if(!p.isEmpty()) qInfo("PNG uloženo: %s", qPrintable(p)); } break;
        default: QWidget::keyPressEvent(e); return;   // neznámou klávesu předej dál
    }
    update();
}
