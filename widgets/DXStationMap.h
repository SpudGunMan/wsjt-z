#ifndef DXSTATIONMAP_H
#define DXSTATIONMAP_H

#include <QWidget>
#include <QString>
#include <QList>
#include <QPointF>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QWheelEvent>
#include <QLabel>
#include <QMenu>
#include <QToolTip>


struct PlottedStation {
    QString call, grid, mode;
    int snr = 0, freqHz = 0, period = 0;
    bool isCQ = false, forMe = false, isLogged = false;
};

class DXStationMap : public QWidget
{
    Q_OBJECT
public:
    explicit DXStationMap(QWidget *parent = nullptr);

    static QString normalizeGrid(QString const& grid);

    void setHomeGrid(QString const& grid);
    void showStation(QString const& call, QString const& grid, int snr,
                     bool isCQ = false, bool forMe = false);
    void clearStations();
    void addStation(PlottedStation const& s);
    void addLoggedStation(QString const& call, QString const& grid, int freqHz, int snr = 0,
                          QString const& mode = QString());
    void tryAddCallsign(QString const& call, int freqHz, int snr, bool forMe);
    void setMyCall(QString const& call);
    void expireStations();  // Enforce hard limit of MAX_STATIONS
    // DXCC/CQ-zone/ITU-zone/continent come from the local country-file lookup
    // (m_logBook), not QRZ.com — mainwindow pushes it in here so this panel is
    // the single place all call info is shown (was previously duplicated in
    // the separate "Call Info" group box).
    void setExtraInfo(QString const& dxcc, QString const& continent, int cqZone, int ituZone);
    // Programmatically select a station (e.g. double-click in Band Activity)
    // so it shows in this panel exactly as if its dot had been clicked.
    void selectStationByCall(QString const& call, QString const& grid, int freqHz = 0, int snr = 0);
    void setDistanceInMiles(bool miles);
    void setStatusMessage(QString const& message);
    void setTickerStats(int loggedQsoTotal, int loggedQsoToday,
                        QDateTime const& startedUtc, QDateTime const& lastLogUtc,
                        double avgDb = 0.0);

signals:
    void stationClicked(QString call, int freqHz, QString grid);
    void stationDoubleClicked(QString call, int freqHz, QString grid);

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    // ── Map geometry ──────────────────────────────────────────────────────────
    static constexpr int INFO_H = 0;    // bottom info panel height (removed in favor of popup tooltip)
    static constexpr int MAX_STATIONS = 300;  // Hard limit on active station list size
    QPointF project(double lon, double lat) const;
    bool    gridToLatLon(QString const& grid, double &lat, double &lon) const;
    double  haversineKm(double, double, double, double) const;
    double  bearingDeg(double, double, double, double) const;
    void    drawGridSquare(QPainter &, QString const&, QColor, QColor, int) const;
    void    drawArc(QPainter &, double, double, double, double) const;
    void    drawHomeMarker(QPainter &) const;
    void    drawStationMarker(QPainter &, double, double, QString const&, QColor) const;
    void    showStationTooltip();        // Show popup with station details
    void    drawGreyline(QPainter &) const;
    void    refreshStatusMessage();
    double  averageLoggedDb() const;
    QString modeSummary() const;

    // ── State ─────────────────────────────────────────────────────────────────
    QString   m_homeGrid;
    QString   m_statusMessage;
    QString   m_unknownGridMessage;
    QString   m_tickerMessage;
    int       m_tickerLoggedTotal = 0;
    int       m_tickerLoggedToday = 0;
    int       m_tickerLogsSinceMidnightUtc = 0;
    QDateTime m_tickerUtcDay;
    double    m_tickerAvgDb = 0.0;
    double    m_tickerDbSum = 0.0;
    int       m_tickerDbCount = 0;
    QMap<QString, int> m_tickerModeCounts;
    QDateTime m_tickerStartedUtc;
    QDateTime m_tickerLastLogUtc;
    double    m_homeLat = 53.0, m_homeLon = -2.0;
    QString   m_selCall, m_selGrid;
    int       m_selSNR = 0, m_selFreqHz = 0;
    double    m_selLat = 0.0, m_selLon = 0.0;
    QList<PlottedStation> m_stations;
    int       m_currentPeriod = 0;
    QPixmap   m_worldMap;
    QPixmap   m_iaruMap;
    bool      m_useIaruMap = false;   // false = world map, true = IARU map
    QString   m_extraDxcc, m_extraContinent;
    int       m_extraCqZone = 0, m_extraItuZone = 0;


    // ── Zoom / pan ────────────────────────────────────────────────────────────
    double  m_zoom = 1.0;
    double  m_panLon = 0.0;
    double  m_panLat = 20.0;
    QPoint  m_dragStartPos;
    double  m_dragPanLon = 0.0;
    double  m_dragPanLat = 20.0;
    bool    m_leftButtonDown = false;   // plain left button currently held
    bool    m_leftIsPanning  = false;   // drag distance exceeded click threshold

    // ── Animation / ticker ─────────────────────────────────────────────────
    QTimer *m_animTimer = nullptr;
    QTimer *m_tickerTimer = nullptr;
    int     m_animFrame = 0;           // increments every 500ms
    double  m_tickerOffset = 0.0;      // smooth animation position for the bottom ticker
    QString m_myCall;                  // used to detect "calling me"

    // ── Callsign→grid cache (so ALL calls can be plotted) ────────────────────
    QMap<QString,QString> m_callGrid;  // call → 4-char grid, built from CQ decodes

    // ── Callsign→SNR cache (preserve SNR through active→logged transition) ───
    QMap<QString,int> m_recentSNR;     // call → most recent SNR value

    // ── Grid visibility ──────────────────────────────────────────────────────
    bool m_showGrid = true;
    bool m_largeTickerFont = false;

    // ── Distance unit ─────────────────────────────────────────────────────────
    bool m_distanceInMiles = false;

    // ── Greyline (daylight) visibility ────────────────────────────────────────
    bool m_showGreyline = true;

    // ── Overlay widgets ───────────────────────────────────────────────────────
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_homeBtn  = nullptr;   // reset zoom/pan to world view
    QPushButton *m_menuBtn  = nullptr;   // hamburger menu for grid toggle
};

#endif
