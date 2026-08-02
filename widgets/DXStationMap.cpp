#include "DXStationMap.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QToolTip>
#include <QRegularExpression>
#include <QDateTime>
#include <QSettings>
#include <QShowEvent>
#include <cmath>

static constexpr double DEG = M_PI / 180.0;

// Valid Maidenhead locator: 2 field letters (A-R) + 2 square digits (0-9),
// optionally + 2 subsquare letters (A-X). Rejects report codes ("R-15", "-09"),
// procedural tokens ("RRR", "73"), and anything else decodedtext's word3/word4
// capture might hand us that isn't actually a grid square.
static const QRegularExpression s_gridRe(
    QStringLiteral("^[A-Ra-r]{2}[0-9]{2}([A-Xa-x]{2})?$"));

static bool isValidGrid(QString const& grid)
{
    return s_gridRe.match(grid).hasMatch();
}

static bool isProceduralGrid(QString const& grid)
{
    const QString value = grid.trimmed().toUpper();
    return value == "RR73" || value == "73" || value == "RRR" || value == "R" || value == "R-" || value == "R+";
}

DXStationMap::DXStationMap(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Station Map");
    setMinimumSize(280, 260);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_worldMap.load(":/worldmap.jpg");
    m_iaruMap.load(":/worldmap_iaru.jpg");

    // ── Restore window geometry from settings ────────────────────────────────
    QSettings settings;
    if (settings.contains("DXStationMap/geometry")) {
        restoreGeometry(settings.value("DXStationMap/geometry").toByteArray());
    } else {
        resize(800, 600);
    }

    // ── Restore grid visibility setting ───────────────────────────────────────
    m_showGrid = settings.value("DXStationMap/showGrid", true).toBool();

    // ── Restore greyline (daylight) visibility setting ─────────────────────────
    m_showGreyline = settings.value("DXStationMap/showGreyline", true).toBool();

    // ── Restore map selection ──────────────────────────────────────────────────
    m_useIaruMap = settings.value("DXStationMap/useIaruMap", false).toBool();

    // ── Restore font size setting ────────────────────────────────────────────────
    m_largeTickerFont = settings.value("DXStationMap/largeTickerFont", false).toBool();

    // ── Hamburger menu — toggle grid visibility ───────────────────────────────
    m_menuBtn = new QPushButton("⋮", this);
    m_menuBtn->setFixedSize(22, 22);
    m_menuBtn->setStyleSheet(
        "QPushButton{background:#0a1828;border:1px solid #1a4060;color:#4a8ab0;"
        "font-size:14px;font-weight:bold;border-radius:3px;padding:0;}"
        "QPushButton:hover{background:#0d2840;color:#00c8ff;}");
    m_menuBtn->setCursor(Qt::ArrowCursor);
    m_menuBtn->move(2, 2);
    connect(m_menuBtn, &QPushButton::clicked, this, [this]() {
        QMenu m;
        m.addAction(m_showGrid ? "Hide Grid" : "Show Grid", [this]() {
            m_showGrid = !m_showGrid;
            update();
        });
        m.addAction(m_showGreyline ? "Hide Daylight" : "Show Daylight", [this]() {
            m_showGreyline = !m_showGreyline;
            update();
        });
        m.addSeparator();
        m.addAction(m_useIaruMap ? "Show World Map" : "Show IARU Map", [this]() {
            m_useIaruMap = !m_useIaruMap;
            update();
        });
        m.addSeparator();
        m.addAction(m_largeTickerFont ? "Larger Font Ticker" : "Default Font Ticker", [this]() {
            m_largeTickerFont = !m_largeTickerFont;
            update();
        });
        m.exec(QCursor::pos());
    });

    // ── Clear button ─────────────────────────────────────────────────────────
    m_clearBtn = new QPushButton(QString(QChar(0x2715)) + " Clear", this);
    m_clearBtn->setFixedSize(62, 20);
    m_clearBtn->setStyleSheet(
        "QPushButton{background:#0a1828;border:1px solid #1a4060;color:#5090b0;"
        "font-size:9px;border-radius:3px;}"
        "QPushButton:hover{background:#0d2840;color:#00c8ff;}");
    m_clearBtn->setCursor(Qt::ArrowCursor);
    connect(m_clearBtn, &QPushButton::clicked, this, &DXStationMap::clearStations);

    // Home button — resets zoom/pan to world view centred on home QTH
    auto *homeBtn = new QPushButton("⌂", this);
    homeBtn->setFixedSize(22, 20);
    homeBtn->setStyleSheet(
        "QPushButton{background:#0a1828;border:1px solid #1a4060;color:#4a8ab0;"
        "font-size:13px;border-radius:3px;}"
        "QPushButton:hover{background:#0d2840;color:#3fb950;}");
    homeBtn->setToolTip("Reset to world view / centre on home QTH");
    homeBtn->move(width() - 91, 2);
    connect(homeBtn, &QPushButton::clicked, this, [this, homeBtn](){
        // Reset zoom to world view, centred on home QTH
        m_zoom = 1.0;
        if (m_homeGrid.length() >= 4) {
            m_panLon = m_homeLon;
            m_panLat = m_homeLat;
        } else {
            m_panLon = 0.0; m_panLat = 20.0;
        }
        homeBtn->move(width()-91, 2);
        update();
    });
    // keep homeBtn repositioned on resize — store as member
    m_homeBtn = homeBtn;

    // Animation timer — drives CQ flash + calling-user radar halo
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(500);
    connect(m_animTimer, &QTimer::timeout, this, [this]{ ++m_animFrame; update(); });
    m_animTimer->start();

    // Ticker timer — refreshes the footer age and rate without involving MainWindow.
    m_tickerTimer = new QTimer(this);
    m_tickerTimer->setInterval(5000);
    connect(m_tickerTimer, &QTimer::timeout, this, [this]() {
        setTickerStats(m_tickerLoggedTotal, m_tickerLoggedToday, m_tickerStartedUtc, m_tickerLastLogUtc);
    });
    m_tickerTimer->start();

    setTickerStats(0, 0, QDateTime::currentDateTimeUtc(), QDateTime(), 0.0);
}

QString DXStationMap::normalizeGrid(QString const& grid)
{
    if (grid.isEmpty()) return {};

    const QString value = grid.trimmed().toUpper();
    if (value.isEmpty()) return {};
    if (isProceduralGrid(value))
        return {};
    if (value.size() < 2) return {};
    if (!s_gridRe.match(value).hasMatch()) return {};
    return value.left(4);
}

void DXStationMap::setMyCall(QString const& call) { m_myCall = call.toUpper(); }

void DXStationMap::setExtraInfo(QString const& dxcc, QString const& continent, int cqZone, int ituZone)
{
    m_extraDxcc = dxcc;
    m_extraContinent = continent;
    m_extraCqZone = cqZone;
    m_extraItuZone = ituZone;
    update();
}

void DXStationMap::setDistanceInMiles(bool miles)
{
    m_distanceInMiles = miles;
    update();
}

void DXStationMap::setStatusMessage(QString const& message)
{
    m_tickerMessage = message;
    refreshStatusMessage();
    update();
}

void DXStationMap::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setTickerStats(m_tickerLoggedTotal, m_tickerLoggedToday, m_tickerStartedUtc, m_tickerLastLogUtc);
}

double DXStationMap::averageLoggedDb() const
{
    return m_tickerDbCount > 0 ? m_tickerDbSum / double(m_tickerDbCount) : m_tickerAvgDb;
}

QString DXStationMap::modeSummary() const
{
    QStringList parts;
    for (auto it = m_tickerModeCounts.constBegin(); it != m_tickerModeCounts.constEnd(); ++it) {
        if (it.value() <= 0) continue;
        parts << QString("%1:%2").arg(it.key()).arg(it.value());
    }
    return parts.isEmpty() ? QString() : QString("modes %1").arg(parts.join(" "));
}

void DXStationMap::setTickerStats(int loggedQsoTotal, int loggedQsoToday,
                                  QDateTime const& startedUtc, QDateTime const& lastLogUtc,
                                  double avgDb)
{
    const QDateTime todayUtc = QDateTime::currentDateTimeUtc().toUTC();
    if (!m_tickerUtcDay.isValid() || m_tickerUtcDay.date() != todayUtc.date()) {
        m_tickerUtcDay = todayUtc;
        m_tickerLogsSinceMidnightUtc = 0;
    }

    m_tickerLoggedTotal = loggedQsoTotal;
    m_tickerLoggedToday = loggedQsoToday;
    if (m_tickerDbCount == 0 && avgDb != 0.0) {
        m_tickerDbSum = avgDb;
        m_tickerDbCount = 1;
    }
    m_tickerAvgDb = averageLoggedDb();
    m_tickerStartedUtc = startedUtc;
    m_tickerLastLogUtc = lastLogUtc;

    QStringList parts;
    const auto nowUtc = QDateTime::currentDateTimeUtc();
    auto formatDuration = [](qint64 seconds) {
        const qint64 hours = seconds / 3600;
        const qint64 minutes = (seconds % 3600) / 60;
        const qint64 secs = seconds % 60;
        if (hours > 0) {
            return QString("%1h%2m%3s").arg(hours).arg(minutes).arg(secs);
        }
        if (minutes > 0) {
            return QString("%1m%2s").arg(minutes).arg(secs);
        }
        return QString("%1s").arg(secs);
    };
    if (m_tickerStartedUtc.isValid()) {
        const qint64 elapsedSeconds = qMax<qint64>(60, m_tickerStartedUtc.secsTo(nowUtc));
        const double elapsedHours = double(elapsedSeconds) / 3600.0;
        const double qsoPerHour = elapsedHours > 0.0 ? double(m_tickerLogsSinceMidnightUtc) / elapsedHours : 0.0;
        parts << QString("QSO/h %1").arg(qsoPerHour, 0, 'f', 1);
        parts << QString("Station Runtime %1").arg(formatDuration(elapsedSeconds));
    }
    parts << QString("avg dB %1").arg(averageLoggedDb(), 0, 'f', 1);
    parts << QString("logs %1/%2").arg(m_tickerLogsSinceMidnightUtc).arg(m_tickerLoggedTotal);
    parts << QString("since 00:UTC %1").arg(m_tickerLogsSinceMidnightUtc);
    const QString summary = modeSummary();
    if (!summary.isEmpty()) {
        parts << summary;
    }
    if (m_tickerLastLogUtc.isValid()) {
        const qint64 ageSeconds = qMax<qint64>(0, m_tickerLastLogUtc.secsTo(nowUtc));
        parts << QString("last log %1 ago").arg(formatDuration(ageSeconds));
    }
    m_tickerMessage = parts.join(" | ");
    refreshStatusMessage();
    update();
}

void DXStationMap::selectStationByCall(QString const& call, QString const& grid, int freqHz, int snr)
{
    m_selCall = call;
    m_selGrid = normalizeGrid(grid);
    m_selSNR = snr;
    m_selFreqHz = freqHz;
    gridToLatLon(m_selGrid, m_selLat, m_selLon);
    update();
}

void DXStationMap::setHomeGrid(QString const& grid)
{
    m_homeGrid = normalizeGrid(grid);
    if (!gridToLatLon(m_homeGrid, m_homeLat, m_homeLon)) {
        // Invalid grid; fallback to defaults
        m_homeLat = 53.0;
        m_homeLon = -2.0;
        m_homeGrid.clear();
    }
    update();
}

void DXStationMap::showStation(QString const& call, QString const& grid, int snr,
                                bool /*isCQ*/, bool /*forMe*/)
{
    m_selCall = call; m_selGrid = normalizeGrid(grid); m_selSNR = snr;
    gridToLatLon(m_selGrid, m_selLat, m_selLon);
    for (auto const& s : m_stations) if (s.call==call) { m_selFreqHz=s.freqHz; break; }
    update();
}

void DXStationMap::refreshStatusMessage()
{
    QStringList unknownCalls;
    for (auto const& station : m_stations) {
        if (!station.call.isEmpty() && station.grid.isEmpty() && !station.isLogged) {
            unknownCalls << station.call;
        }
    }
    m_unknownGridMessage = unknownCalls.isEmpty() ? QString() : QString("Waiting for grid: %1").arg(unknownCalls.join(", "));

    QStringList parts;
    if (!m_unknownGridMessage.isEmpty()) {
        parts << m_unknownGridMessage;
    }
    if (!m_tickerMessage.isEmpty()) {
        parts << m_tickerMessage;
    }
    m_statusMessage = parts.join(" | ");
}

void DXStationMap::clearStations()
{
    m_stations.clear();
    m_callGrid.clear();
    m_recentSNR.clear();
    m_tickerDbSum = 0.0;
    m_tickerDbCount = 0;
    m_tickerAvgDb = 0.0;
    m_tickerModeCounts.clear();
    m_selCall.clear(); m_selGrid.clear();
    m_selSNR=0; m_selFreqHz=0; m_selLat=0.0; m_selLon=0.0;
    refreshStatusMessage();
    update();
}

void DXStationMap::addStation(PlottedStation const& s)
{
    PlottedStation updated = s;
    const QString normalizedGrid = normalizeGrid(s.grid);
    const bool proceduralSignoff = isProceduralGrid(s.grid);

    QString existingGrid;
    for (auto const& existing : m_stations) {
        if (existing.call == s.call) {
            existingGrid = existing.grid;
            break;
        }
    }

    if (!normalizedGrid.isEmpty()) {
        updated.grid = normalizedGrid;
        m_callGrid[s.call] = updated.grid;
    } else if (proceduralSignoff) {
        updated.grid = existingGrid.isEmpty() ? m_callGrid.value(s.call) : existingGrid;
    } else if (m_callGrid.contains(s.call)) {
        updated.grid = m_callGrid.value(s.call);
    } else {
        updated.grid = existingGrid;
    }

    if (proceduralSignoff) {
        updated.forMe = false;
        updated.isLogged = false;
    } else if (updated.grid.isEmpty()) {
        updated.forMe = false;
        updated.isLogged = updated.isLogged || proceduralSignoff;
    }
    m_recentSNR[s.call] = updated.snr;

    for (auto &e : m_stations) {
        if (e.call == updated.call) {
            e = updated;
            if (!e.grid.isEmpty()) m_callGrid[e.call] = e.grid;
            refreshStatusMessage();
            update();
            return;
        }
    }

    if (!updated.grid.isEmpty()) {
        m_stations.append(updated);
    }
    refreshStatusMessage();
    update();
}

void DXStationMap::addLoggedStation(QString const& call, QString const& grid, int freqHz, int snr,
                                    QString const& mode)
{
    if (call.isEmpty()) return;

    const QString normalizedGrid = normalizeGrid(grid);
    QString effectiveGrid = normalizedGrid;

    for (auto &e : m_stations) {
        if (e.call == call) {
            if (effectiveGrid.isEmpty() && !e.grid.isEmpty()) effectiveGrid = e.grid;
            e.isLogged = true;
            e.freqHz = freqHz;
            e.snr = (snr != 0) ? snr : e.snr;
            e.grid = effectiveGrid;
            e.mode = mode;
            if (!mode.isEmpty()) {
                ++m_tickerModeCounts[mode];
            }
            const QDateTime todayUtc = QDateTime::currentDateTimeUtc().toUTC();
            if (!m_tickerUtcDay.isValid() || m_tickerUtcDay.date() != todayUtc.date()) {
                m_tickerUtcDay = todayUtc;
                m_tickerLogsSinceMidnightUtc = 0;
            }
            ++m_tickerLogsSinceMidnightUtc;
            if (snr != 0) {
                m_tickerDbSum += snr;
                ++m_tickerDbCount;
            }
            if (!effectiveGrid.isEmpty()) m_callGrid[call] = effectiveGrid;
            refreshStatusMessage();
            update();
            return;
        }
    }

    PlottedStation s;
    s.call = call;
    s.grid = effectiveGrid;
    s.freqHz = freqHz;
    s.isLogged = true;
    s.isCQ = false;
    s.forMe = false;
    s.snr = (snr != 0) ? snr : m_recentSNR.value(call, 0);
    if (s.snr != 0) {
        m_tickerDbSum += s.snr;
        ++m_tickerDbCount;
    }
    if (!mode.isEmpty()) {
        ++m_tickerModeCounts[mode];
    }
    const QDateTime todayUtc = QDateTime::currentDateTimeUtc().toUTC();
    if (!m_tickerUtcDay.isValid() || m_tickerUtcDay.date() != todayUtc.date()) {
        m_tickerUtcDay = todayUtc;
        m_tickerLogsSinceMidnightUtc = 0;
    }
    ++m_tickerLogsSinceMidnightUtc;
    s.mode = mode;
    s.period = 0;
    addStation(s);
}

// Try to plot a callsign that appeared in a non-CQ message using cached grid
void DXStationMap::tryAddCallsign(QString const& call, int freqHz, int snr, bool forMe)
{
    if (call.isEmpty()) return;
    for (auto const& s : m_stations) if (s.call==call) return;

    const QString cachedGrid = normalizeGrid(m_callGrid.value(call));
    PlottedStation ps;
    ps.call=call; ps.grid=cachedGrid; ps.freqHz=freqHz;
    ps.snr=snr; ps.period=m_currentPeriod; ps.isCQ=false; ps.forMe=forMe;
    m_stations.append(ps);
    refreshStatusMessage();
    update();
}

void DXStationMap::expireStations(int currentPeriod, int maxAge)
{
    m_currentPeriod = currentPeriod;
    for (int i=m_stations.size()-1; i>=0; --i)
        if ((currentPeriod-m_stations[i].period) > maxAge) m_stations.removeAt(i);
    update();
}

// ── Projection ────────────────────────────────────────────────────────────────
QPointF DXStationMap::project(double lon, double lat) const
{
    const int mapH = height() - INFO_H;
    const int w = width();
    // Apply zoom centred on m_panLon/m_panLat
    const double cx = (m_panLon + 180.0) / 360.0 * w;
    const double cy = (90.0 - m_panLat) / 180.0  * mapH;
    const double x  = (lon + 180.0) / 360.0 * w;
    const double y  = (90.0 - lat)  / 180.0  * mapH;
    return { cx + (x - cx) * m_zoom, cy + (y - cy) * m_zoom };
}

bool DXStationMap::gridToLatLon(QString const& grid, double &lat, double &lon) const
{
    if (!isValidGrid(grid)) return false;
    const auto g = grid.toUpper();
    lon = (g[0].unicode()-'A')*20.0-180.0;
    lat = (g[1].unicode()-'A')*10.0-90.0;
    if (g.length()>=4) { lon+=(g[2].unicode()-'0')*2.0; lat+=(g[3].unicode()-'0'); }
    lon += g.length()>=4 ? 1.0 : 10.0;
    lat += g.length()>=4 ? 0.5 : 5.0;
    return true;
}

double DXStationMap::haversineKm(double lat1,double lon1,double lat2,double lon2) const
{
    const double R=6371;
    const double dLat=(lat2-lat1)*DEG, dLon=(lon2-lon1)*DEG;
    const double a=sin(dLat/2)*sin(dLat/2)+cos(lat1*DEG)*cos(lat2*DEG)*sin(dLon/2)*sin(dLon/2);
    return R*2*atan2(sqrt(a),sqrt(1-a));
}

double DXStationMap::bearingDeg(double lat1,double lon1,double lat2,double lon2) const
{
    const double y=sin((lon2-lon1)*DEG)*cos(lat2*DEG);
    const double x=cos(lat1*DEG)*sin(lat2*DEG)-sin(lat1*DEG)*cos(lat2*DEG)*cos((lon2-lon1)*DEG);
    return fmod(atan2(y,x)/DEG+360.0,360.0);
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
void DXStationMap::drawGridSquare(QPainter &p, QString const& grid,
                                   QColor fill, QColor stroke, int sw) const
{
    if (grid.length()<2) return;
    const auto g=grid.toUpper();
    double lon0=(g[0].unicode()-'A')*20.0-180.0, lat0=(g[1].unicode()-'A')*10.0-90.0;
    double lon1, lat1;
    if (g.length()>=4) { lon0+=(g[2].unicode()-'0')*2; lat0+=(g[3].unicode()-'0'); lon1=lon0+2; lat1=lat0+1; }
    else               { lon1=lon0+20; lat1=lat0+10; }
    p.setBrush(fill); p.setPen(QPen(stroke,sw));
    p.drawRect(QRectF(project(lon0,lat1),project(lon1,lat0)).normalized());
}

void DXStationMap::drawArc(QPainter &p,double lat1,double lon1,double lat2,double lon2) const
{
    QPainterPath path; bool first=true;
    for (int i=0;i<=60;++i) {
        const double t=double(i)/60;
        const double d=2*asin(sqrt(pow(sin((lat2-lat1)*DEG/2),2)+cos(lat1*DEG)*cos(lat2*DEG)*pow(sin((lon2-lon1)*DEG/2),2)));
        if (d<1e-6) break;
        const double A=sin((1-t)*d)/sin(d), B=sin(t*d)/sin(d);
        const double x=A*cos(lat1*DEG)*cos(lon1*DEG)+B*cos(lat2*DEG)*cos(lon2*DEG);
        const double y=A*cos(lat1*DEG)*sin(lon1*DEG)+B*cos(lat2*DEG)*sin(lon2*DEG);
        const double z=A*sin(lat1*DEG)+B*sin(lat2*DEG);
        const double la=atan2(z,sqrt(x*x+y*y))/DEG, lo=atan2(y,x)/DEG;
        const QPointF pt=project(lo,la);
        if (first) { path.moveTo(pt); first=false; } else { path.lineTo(pt); }
    }
    QPen pen(QColor(77,200,255),1.5,Qt::DashLine);
    pen.setDashPattern({5.0,3.0});
    p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawPath(path);
}

// Day/night terminator ("greyline"). Approximate solar position — good
// enough for a visual overlay, not precision astronomy:
//   declination via the standard -23.44*cos(360/365*(dayOfYear+10)) fit
//   sub-solar longitude from the UTC time of day (solar noon there now)
// A point is in daylight when its solar elevation > 0:
//   sin(elev) = sin(lat)*sin(decl) + cos(lat)*cos(decl)*cos(hourAngle)
// hourAngle here is just (this point's longitude - sub-solar longitude),
// in degrees, since both advance 15 deg/hour together.
void DXStationMap::drawGreyline(QPainter &p) const
{
    auto const utcNow = QDateTime::currentDateTimeUtc();
    auto const& date = utcNow.date();
    auto const& time = utcNow.time();
    const int dayOfYear = date.dayOfYear();
    const double hourUtc = time.hour() + time.minute()/60.0 + time.second()/3600.0;

    const double declDeg = -23.44 * std::cos(DEG * (360.0/365.0) * (dayOfYear + 10));
    const double declRad = declDeg * DEG;
    // Longitude currently at local solar noon (~180 - hourUtc*15, wrapped)
    double subsolarLon = 180.0 - hourUtc * 15.0;
    while (subsolarLon > 180.0)  subsolarLon -= 360.0;
    while (subsolarLon < -180.0) subsolarLon += 360.0;

    const double lonStepDeg = 7.0, latStepDeg = 7.0;
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::Antialiasing, true);
    
    // Draw blocks with antialiasing and smooth twilight gradient
    for (double lon = -180.0; lon < 180.0; lon += lonStepDeg) {
        const double hourAngleRad = (lon - subsolarLon) * DEG;
        const double cosH = std::cos(hourAngleRad);
        for (double lat = -87.0; lat < 87.0; lat += latStepDeg) {
            const double latRad = lat * DEG;
            const double sinElev = std::sin(latRad)*std::sin(declRad)
                                  + std::cos(latRad)*std::cos(declRad)*cosH;
            if (sinElev >= 0.0) continue;   // daylight — leave clear
            
            // Very smooth alpha: full opacity at night, wide fade through twilight band
            const double alpha = qBound(0.0, -sinElev / 0.15, 1.0);
            
            QPointF const c1 = project(lon, lat);
            QPointF const c2 = project(lon + lonStepDeg, lat + latStepDeg);
            const double cw = std::abs(c2.x() - c1.x()) + 1.0;
            const double ch = std::abs(c2.y() - c1.y()) + 1.0;
            
            // Antialiased blocks with smooth overlay
            p.fillRect(QRectF(c1.x(), c1.y(), cw, ch), QColor(20, 20, 40, int(alpha * 140)));
        }
    }
}

void DXStationMap::drawHomeMarker(QPainter &p) const
{
    // Guard: require a valid 4+ char locator that has been explicitly set by setHomeGrid()
    // Prevents drawing at the constructor defaults (53.0, -2.0) before the real QTH is known.
    if (m_homeGrid.length() < 4) return;
    const QPointF pt=project(m_homeLon,m_homeLat);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setBrush(QColor(100,150,255)); p.setPen(QPen(QColor(40,100,220),1));
    p.drawEllipse(pt,5,5);
    p.setFont(QFont("sans-serif",8,QFont::Bold));
    p.setPen(QColor(160,210,255));
    p.drawText(QPointF(pt.x()+7,pt.y()-4), m_homeGrid.left(4));
    p.setRenderHint(QPainter::Antialiasing,false);
}

void DXStationMap::drawStationMarker(QPainter &p,double lat,double lon,
                                      QString const& lbl,QColor col) const
{
    const QPointF pt=project(lon,lat);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setBrush(col); p.setPen(QPen(col.darker(150),1));
    p.drawEllipse(pt,6,6);
    p.setFont(QFont("Courier New",8,QFont::Bold));
    p.setPen(col);
    p.drawText(QPointF(pt.x()+8,pt.y()-5),lbl.left(12));
    p.setRenderHint(QPainter::Antialiasing,false);
}

// ── Popup tooltip with station details ─────────────────────────────────────
void DXStationMap::showStationTooltip()
{
    if (m_selCall.isEmpty()) return;

    // Build tooltip text with station details
    const double km  = haversineKm(m_homeLat, m_homeLon, m_selLat, m_selLon);
    const double brg = bearingDeg(m_homeLat, m_homeLon, m_selLat, m_selLon);
    
    QString text = QString("<b>%1</b><br>").arg(m_selCall);
    text += QString("Grid: %1<br>").arg(m_selGrid.isEmpty() ? QString("unknown") : m_selGrid);
    text += QString("SNR: %1 dB<br>").arg(m_selSNR);
    
    if (m_distanceInMiles) {
        text += QString("Dist: %1 mi / %2°<br>")
                .arg(int(km*0.621371+0.5)).arg(int(brg+0.5));
    } else {
        text += QString("Dist: %1 km / %2°<br>")
                .arg(int(km+0.5)).arg(int(brg+0.5));
    }
    
    if (!m_extraDxcc.isEmpty())      text += QString("DXCC: %1<br>").arg(m_extraDxcc);
    if (!m_extraContinent.isEmpty()) text += QString("Cont: %1<br>").arg(m_extraContinent);
    if (m_extraCqZone > 0)           text += QString("CQ/ITU: %1 / %2").arg(m_extraCqZone).arg(m_extraItuZone);

    QToolTip::showText(QCursor::pos(), text, this);
}

// ── paintEvent ───────────────────────────────────────────────────────────────
void DXStationMap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int w=width(), h=height(), mapH=h-INFO_H;
    if (w<20||mapH<20) return;

    // Select the appropriate map based on user preference
    const QPixmap& currentMap = m_useIaruMap ? m_iaruMap : m_worldMap;

    // Map background — apply zoom via inverse projection
    if (!currentMap.isNull()) {
        const int iw = currentMap.width(), ih = currentMap.height();
        if (m_zoom <= 1.01) {
            // World view — full image
            p.drawPixmap(QRect(0,0,w,mapH), currentMap, QRect(0,0,iw,ih));
        } else {
            // Zoomed — compute which portion of the map is visible.
            // Inverse of project(): world_norm = pan_norm + (screen/size - pan_norm)/zoom
            const double pnx = (m_panLon + 180.0) / 360.0;
            const double pny = (90.0 - m_panLat) / 180.0;
            // Visible normalised lat/lon bounds at zoom level
            const double x0n = pnx + (0.0 - pnx) / m_zoom;   // left  edge (screen x=0)
            const double x1n = pnx + (1.0 - pnx) / m_zoom;   // right edge (screen x=w)
            const double y0n = pny + (0.0 - pny) / m_zoom;   // top   edge (screen y=0)
            const double y1n = pny + (1.0 - pny) / m_zoom;   // bot   edge (screen y=mapH)
            // Clamp to [0,1] and convert to image pixels
            const int sx = qBound(0, int(x0n * iw), iw);
            const int sy = qBound(0, int(y0n * ih), ih);
            const int sw = qBound(1, int((x1n - x0n) * iw), iw - sx);
            const int sh = qBound(1, int((y1n - y0n) * ih), ih - sy);
            p.drawPixmap(QRect(0,0,w,mapH), currentMap, QRect(sx,sy,sw,sh));
        }
        p.fillRect(0,0,w,mapH,QColor(0,0,0,45));
    } else {
        for (int y=0;y<mapH;++y) {
            const double t=double(y)/mapH, s=4*t*(1-t);
            p.fillRect(0,y,w,1,QColor(int(3+s*14),int(7+s*20),int(18+s*38)));
        }
    }

    if (m_showGreyline) drawGreyline(p);

    // Grid lines — subtle over photo
    const bool hasSat=!currentMap.isNull();
    const int cellW=w/18, cellH=mapH/18;

    if (m_showGrid) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(hasSat?QColor(80,140,200,40):QColor(18,55,100,200),1));
        for (int i=0;i<=18;++i) { const double lon=i*20.0-180; p.drawLine(project(lon,-90),project(lon,90)); }
        for (int j=0;j<=18;++j) { const double lat=j*10.0-90;  p.drawLine(project(-180,lat),project(180,lat)); }

        // Field labels when no satellite image
        const int fs=qBound(4,cellH/2-1,cellW/3);
        if (!hasSat&&cellH>=12&&fs>=4) {
            p.setFont(QFont("sans-serif",fs)); p.setPen(QColor(30,80,145,170));
            const QFontMetrics fm(p.font());
            for (int fi=0;fi<18;++fi) for (int li=0;li<18;++li) {
                const QPointF c=project(fi*20.0-180+10, li*10.0-90+5);
                if (c.x()<0||c.x()>w||c.y()<0||c.y()>mapH) continue;
                const QString lbl=QString("%1%2").arg(QChar('A'+fi)).arg(QChar('A'+li));
                p.drawText(QPointF(c.x()-fm.horizontalAdvance(lbl)/2.0,c.y()+fm.ascent()/2.0),lbl);
            }
        }

        // Equator / prime meridian
        p.setPen(QPen(QColor(30,90,160,80),1,Qt::DashLine));
        p.drawLine(project(-180,0),project(180,0));
        p.drawLine(project(0,-90),project(0,90));
    }

    // 4-char minor square only — one precise rectangle, no nested field boxes
    if (!m_selGrid.isEmpty() && !m_selCall.isEmpty() && m_selGrid.length()>=4) {
        // Subtle field tint — 20°×10° area, very faint so it doesn't dominate
        drawGridSquare(p, m_selGrid.left(2), QColor(40,100,180,15), QColor(40,100,180,30), 0);
        // Precise 4-char minor square — clear bright border
        drawGridSquare(p, m_selGrid,           QColor(77,166,255,60), QColor(100,200,255), 2);
    }

    // All station dots with animation
    const double dotR = qMax(3.0, cellW * 0.12);
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool animOn = (m_animFrame & 1);   // toggles every 500ms

    for (auto const& s : m_stations) {
        if (s.call == m_selCall) continue;
        double lat = 0.0, lon = 0.0;
        const bool hasGrid = !s.grid.isEmpty() && gridToLatLon(s.grid, lat, lon);
        if (!hasGrid) {
            continue;
        }
        const QPointF pt = project(lon, lat);

        // --- Unified SNR Color Calculation ---
        QColor snrCol;
        if (s.snr < -10)      snrCol = QColor(204, 85, 0);   // Dark Orange (-30 to -10)
        else if (s.snr < 0)   snrCol = QColor(255, 165, 0);  // Orange (-10 to 0)
        else if (s.snr < 10)  snrCol = QColor(173, 255, 47); // Lime Green (0 to 10)
        else if (s.snr < 20)  snrCol = QColor(50, 205, 50);  // Grass Green (10 to 20)
        else                  snrCol = QColor(34, 139, 34);   // Forest Green (20 to 30)

        if (s.isLogged) {
            // ── Logged QSO: Signal-color based ---
            p.setBrush(snrCol); 
            p.setPen(Qt::NoPen);
            p.drawEllipse(pt, dotR, dotR);
            p.setFont(QFont("Courier New", 7));
            p.drawText(QPointF(pt.x()+dotR+2, pt.y()-3), s.call.left(10));

        } else if (s.forMe && !s.grid.isEmpty()) {
            // ── Calling ME: Signal-color center + pulsing halo ---
            if (animOn) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(255,60,60,120), 1)); // Outer alert ring red
                p.drawEllipse(pt, dotR*3.5, dotR*3.5);
                p.setPen(QPen(QColor(255,80,80,60), 1));
                p.drawEllipse(pt, dotR*5, dotR*5);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(snrCol); // Use the SNR color for the center point
            p.drawEllipse(pt, dotR*1.4, dotR*1.4);
            p.setFont(QFont("Courier New", 8, QFont::Bold));
            p.setPen(QColor(255,200,200));
            p.drawText(QPointF(pt.x()+dotR*1.5, pt.y()-4), s.call.left(12));

        } else if (s.isCQ) {
            // ── CQ: Signal-color based ---
            p.setBrush(snrCol);
            p.setPen(Qt::NoPen);
            p.drawEllipse(pt, dotR, dotR);
            p.setFont(QFont("Courier New", 7));
            p.setPen(QColor(160,210,255)); // Keep light blue-ish label
            p.drawText(QPointF(pt.x()+dotR+2, pt.y()-3), s.call.left(10));

        } else {
            // ── Other directed messages (SNR Color Mapped) ---
            p.setBrush(snrCol);
            p.setPen(Qt::NoPen);
            p.drawEllipse(pt, dotR*0.85, dotR*0.85);
        }
    }
    p.setRenderHint(QPainter::Antialiasing, false);

    if (!m_statusMessage.isEmpty()) {
        const int tickerFontSize = m_largeTickerFont ? 24 : 12;
        const int tickerBarHeight = tickerFontSize + 8;
        const int tickerBarY = h - tickerBarHeight;
        QRect statusRect(0, tickerBarY, w, tickerBarHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 140));
        p.drawRect(statusRect);
        p.setFont(QFont(QStringLiteral("sans-serif"), tickerFontSize));
        const QFontMetrics fm(p.font());
        const int textWidth = fm.horizontalAdvance(m_statusMessage);
        const int travel = qMax(1, textWidth + w + 20);
        const double step = 6.0;
        m_tickerOffset += step;
        if (m_tickerOffset > travel) {
            m_tickerOffset = 0.0;
        }
        const int offset = int(m_tickerOffset);
        const int x = w + 20 - offset;
        p.setPen(QColor(220, 240, 255));
        p.drawText(QRect(x, tickerBarY, textWidth, tickerBarHeight), Qt::AlignLeft | Qt::AlignVCenter, m_statusMessage);
    }

    // Arc + markers
    if (!m_selGrid.isEmpty()&&!m_homeGrid.isEmpty()&&!m_selCall.isEmpty())
        drawArc(p,m_homeLat,m_homeLon,m_selLat,m_selLon);
    drawHomeMarker(p);
    if (!m_selCall.isEmpty())
        drawStationMarker(p,m_selLat,m_selLon,m_selCall,QColor(255,100,80));
}

// ── Events ────────────────────────────────────────────────────────────────────
void DXStationMap::wheelEvent(QWheelEvent *e)
{
    const double delta = e->angleDelta().y() > 0 ? 1.25 : 0.80;
    const int mapH = height() - INFO_H;
    const double w = width();

    // Compute world position under cursor using CURRENT zoom (inverse-project).
    // cnx/cny are normalised world coords (0..1 range).
    const double pnx_old = (m_panLon + 180.0) / 360.0;
    const double pny_old = (90.0 - m_panLat) / 180.0;
    const double sx = e->position().x(), sy = e->position().y();
    const double cnx = pnx_old + (sx / w     - pnx_old) / m_zoom;
    const double cny = pny_old + (sy / mapH   - pny_old) / m_zoom;

    m_zoom = qBound(1.0, m_zoom * delta, 8.0);

    if (m_zoom <= 1.01) {
        m_panLon = 0.0; m_panLat = 20.0;
    } else {
        // Solve for new pan that keeps the cursor on the same world point.
        // From project(): sx/w = pnx_new + (cnx - pnx_new) * zoom_new
        // => pnx_new = (sx/w - cnx * zoom_new) / (1 - zoom_new)
        const double pnx_new = (sx / w     - cnx * m_zoom) / (1.0 - m_zoom);
        const double pny_new = (sy / mapH  - cny * m_zoom) / (1.0 - m_zoom);
        m_panLon = qBound(-180.0, pnx_new * 360.0 - 180.0, 180.0);
        m_panLat = qBound( -85.0, 90.0 - pny_new * 180.0,   85.0);
    }
    update();
}

void DXStationMap::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_menuBtn)  m_menuBtn->move(2, 2);
    if (m_clearBtn) m_clearBtn->move(width()-66, 2);
    if (m_homeBtn)  m_homeBtn->move(width()-91, 2);
    update();
}

void DXStationMap::closeEvent(QCloseEvent *e)
{
    QSettings settings;
    settings.setValue("DXStationMap/geometry", saveGeometry());
    settings.setValue("DXStationMap/showGrid", m_showGrid);
    settings.setValue("DXStationMap/showGreyline", m_showGreyline);
    settings.setValue("DXStationMap/useIaruMap", m_useIaruMap);
    settings.setValue("DXStationMap/largeTickerFont", m_largeTickerFont);
    QWidget::closeEvent(e);
}

// Screen-pixel drag distance beyond which a left-button press is treated as a
// pan gesture rather than a station click.
static constexpr int PAN_CLICK_THRESHOLD_PX = 4;

void DXStationMap::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button()==Qt::MiddleButton || (e->button()==Qt::LeftButton && (e->modifiers()&Qt::ControlModifier)))
        setCursor(Qt::ArrowCursor);

    if (e->button()==Qt::LeftButton && m_leftButtonDown) {
        const bool wasPanning = m_leftIsPanning;
        m_leftButtonDown = false;
        m_leftIsPanning  = false;
        setCursor(Qt::ArrowCursor);

        // Only treat as a station click if the press never turned into a drag.
        if (!wasPanning) {
            const int mapH = height() - INFO_H;
            if (e->pos().y() <= mapH) {
                double minDist=18; PlottedStation const *found=nullptr;
                for (auto const& s:m_stations) {
                    double lat,lon; if (!gridToLatLon(s.grid,lat,lon)) continue;
                    const QPointF pt=project(lon,lat);
                    const double d=hypot(e->pos().x()-pt.x(),e->pos().y()-pt.y());
                    if (d<minDist) { minDist=d; found=&s; }
                }
                if (found) {
                    m_selCall=found->call; m_selGrid=found->grid;
                    m_selSNR=found->snr;  m_selFreqHz=found->freqHz;
                    gridToLatLon(m_selGrid,m_selLat,m_selLon);
                    update();
                    emit stationClicked(found->call,found->freqHz,found->grid);
                    showStationTooltip();  // Show popup with station details
                } else {
                    // Click on empty space — clear selection and arc line
                    m_selCall.clear(); m_selGrid.clear();
                    update();
                }
            }
        }
    }
    QWidget::mouseReleaseEvent(e);
}

void DXStationMap::mousePressEvent(QMouseEvent *e)
{
    // Pan: middle-click drag OR Ctrl+left-drag (immediate)
    if (e->button()==Qt::MiddleButton ||
        (e->button()==Qt::LeftButton && (e->modifiers()&Qt::ControlModifier))) {
        m_dragStartPos=e->pos(); m_dragPanLon=m_panLon; m_dragPanLat=m_panLat;
        setCursor(Qt::ClosedHandCursor); return;
    }
    if (e->button()!=Qt::LeftButton) return;

    // Plain left button: record a pan candidate. Whether this ends up being a
    // click (station select, handled in mouseReleaseEvent) or a drag (pan,
    // handled in mouseMoveEvent) is decided by movement distance.
    m_dragStartPos = e->pos();
    m_dragPanLon = m_panLon; m_dragPanLat = m_panLat;
    m_leftButtonDown = true;
    m_leftIsPanning  = false;
}

void DXStationMap::mouseMoveEvent(QMouseEvent *e)
{
    const int mapH=height()-INFO_H;

    const bool ctrlLeftDrag = e->buttons()&Qt::LeftButton && (e->modifiers()&Qt::ControlModifier);
    const bool middleDrag   = e->buttons()&Qt::MiddleButton;
    const bool plainLeftDrag = m_leftButtonDown && (e->buttons()&Qt::LeftButton) && !ctrlLeftDrag;

    if (plainLeftDrag && !m_leftIsPanning) {
        const double dist = hypot(e->pos().x()-m_dragStartPos.x(), e->pos().y()-m_dragStartPos.y());
        if (dist > PAN_CLICK_THRESHOLD_PX) {
            m_leftIsPanning = true;
            setCursor(Qt::ClosedHandCursor);
        }
    }

    if (middleDrag || ctrlLeftDrag || (plainLeftDrag && m_leftIsPanning)) {
        const double dx=e->pos().x()-m_dragStartPos.x();
        const double dy=e->pos().y()-m_dragStartPos.y();
        m_panLon=qBound(-180.0, m_dragPanLon-dx*360.0/width()/m_zoom, 180.0);
        m_panLat=qBound( -85.0, m_dragPanLat+dy*180.0/(height()-INFO_H)/m_zoom, 85.0);
        update(); return;
    }

    if (e->pos().y()>=mapH) return;
    const double lon=(double(e->pos().x())/width())*360.0-180.0;
    const double lat=90.0-(double(e->pos().y())/mapH)*180.0;
    const int fi=int((lon+180)/20), li=int((lat+90)/10);
    if (fi>=0&&fi<18&&li>=0&&li<18) {
        QString lbl; lbl+=QChar('A'+fi); lbl+=QChar('A'+li);
        lbl+=QChar('0'+int(fmod(lon+180,20)/2));
        lbl+=QChar('0'+int(fmod(lat+90, 10)));
        QToolTip::showText(e->globalPos(),lbl,this);
    }
    QWidget::mouseMoveEvent(e);
}
