// -*- Mode: C++ -*-
#ifndef WIDEGRAPH_H_
#define WIDEGRAPH_H_

#include <QDialog>
#include <QScopedPointer>
#include <QList>
#include <QTimer>
#include <QDir>
#include <QHash>
#include <QVariant>
#include "WFPalette.hpp"
#include "decode_label.h"

#define MAX_SCREENSIZE 2048

namespace Ui {
  class WideGraph;
}

class QSettings;
class Configuration;

class WideGraph : public QDialog
{
  Q_OBJECT

public:
  explicit WideGraph(QSettings *, QWidget *parent = 0);
  ~WideGraph ();

  void   dataSink2(float s[], float df3, int ihsym, int ndiskdata, float pdB);
  void   setRxFreq(int n);
  int    rxFreq();
  int    nStartFreq();
  int    Fmin();
  int    Fmax();
  int    fSpan();
  void   saveSettings();
  void   setFsample(int n);
  void   setPeriod(double trperiod, int nsps);
  void   setTxFreq(int n);
  void   setMode(QString mode);
  void   setSubMode(int n);
  bool   flatten();
  bool   useRef();
  void   setTol(int n);
  void   setSuperFox(bool b);
  void   setSuperHound(bool b);
  int    smoothYellow();
  void   setRxBand (QString const& band);
  void   setWSPRtransmitted();
  void   drawRed(int ia, int ib);
  void   setVHF(bool bVHF);
  void   setRedFile(QString fRed);
  void   setFST4_FreqRange(int fLow,int fHigh);
  void   setSingleDecode(bool b);
  void   setDiskUTC(int nutc);
  void   restartTotalPower();
  void   setDarkStyle(bool b);

  // Decoded-callsign overlay (N6NU port from QMAP 2026-05-11).
  // DisplayText emits decodedCallsign(freq_hz, callsign, is_cq) on
  // every decode line; MainWindow connects that to addDecodeLabel here.
  // We dedupe by callsign, age out after m_decodeLabelPeriods × TR
  // period, and push the live list to the plotter for rendering.
  void   addDecodeLabel(double freq_hz, const QString& callsign,
                        bool is_cq, int time_sec);
  bool   decodeLabelsEnabled() const { return m_decodeLabelsEnabled; }
  void   setDecodeLabelsEnabled(bool on);
  int    decodeLabelPeriods() const { return m_decodeLabelPeriods; }
  void   setDecodeLabelPeriods(int n);
  // Wipe overlay when operator switches mode (FT8→FT4 etc.) -
  // stale callsigns at FT8 audio offsets are meaningless in FT4 framing,
  // and would just clutter the new mode.
  void   clearDecodeLabels();
  // Track WSJT-X's "DX Call" entry → mark the matching label as
  // active (light-green background, contrasting text). Empty string
  // clears the active marker.
  void   setActiveCallsign(const QString& call);
  DecodeLabelFontSize decodeLabelFontSize() const { return m_decodeFontSize; }
  void   setDecodeLabelFontSize(DecodeLabelFontSize sz);
  // Overlay transparency. View menu offers None=255 / Medium=200 /
  // High=175; persisted in WideGraph QSettings group as DecodeLabelAlpha.
  int    decodeLabelAlpha() const { return m_decodeLabelAlpha; }
  void   setDecodeLabelAlpha(int alpha);

signals:
  void freezeDecode2(int n);
  void f11f12(int n);
  void setXIT2(int n);
  void setFreq3(int rxFreq, int txFreq);
  // Emitted when the decode-overlay enable state changes, so the View
  // menu mirror in MainWindow stays in sync.
  void decodeLabelsEnabledChanged(bool on);
  // Emitted when the decode-overlay periods-to-keep changes (from the
  // WideGraph spin box or the View menu radio items), so the other
  // surface can sync without polling.
  void decodeLabelPeriodsChanged(int n);

public slots:
  void wideFreezeDecode(int n);
  void setFreq2(int rxFreq, int txFreq);
  void setDialFreq(double d);

protected:
  void keyPressEvent (QKeyEvent *e) override;
  void closeEvent (QCloseEvent *) override;

private slots:
  void on_waterfallAvgSpinBox_valueChanged(int arg1);
  void on_bppSpinBox_valueChanged(int arg1);
  void on_spec2dComboBox_currentIndexChanged(int);
  void on_fSplitSpinBox_valueChanged(int n);
  void on_fStartSpinBox_valueChanged(int n);
  void on_paletteComboBox_activated(const QString &palette);
  void on_cbFlatten_toggled(bool b);
  void on_cbRef_toggled(bool b);
  void on_cbControls_toggled(bool b);
  void on_cbBars_toggled(bool b);
  void on_cbFreq_toggled(bool b);
  void on_cbShowCallsigns_toggled(bool b);
  void on_sbDecodeLabelPeriods_valueChanged(int n);
  void on_pbClearDecodeLabels_clicked();
  void on_timestampComboBox_currentIndexChanged(int n);
  void on_adjust_palette_push_button_clicked (bool);
  void on_gainSlider_valueChanged(int value);
  void on_zeroSlider_valueChanged(int value);
  void on_gain2dSlider_valueChanged(int value);
  void on_zero2dSlider_valueChanged(int value);
  void on_smoSpinBox_valueChanged(int n);  
  void on_sbPercent2dPlot_valueChanged(int n);

private:
  void readPalette ();
  void setRxRange ();
  void replot();

  QScopedPointer<Ui::WideGraph> ui;

  QSettings * m_settings;
  QDir m_palettes_path;
  WFPalette m_userPalette;
  QHash<QString, QVariant> m_fMinPerBand;

  double m_tr0;
  double m_TRperiod;

  qint32 m_waterfallAvg;
  qint32 m_nsps;
  qint32 m_fMax;
  qint32 m_nSubMode;
  qint32 m_nsmo;
  qint32 m_Percent2DScreen;
  qint32 m_jz=MAX_SCREENSIZE;
  qint32 m_n;
  // Z
  bool	 m_bars;
  bool	 m_clear;
  bool   m_freq;
  qint32 m_timestamp;
  bool   m_bFlatten;
  bool   m_bRef;
  bool   m_bHaveTransmitted;    //Set true at end of a WSPR or FT4 transmission

  QString m_rxBand;
  QString m_mode;
  QString m_waterfallPalette;
  float   m_swide[MAX_SCREENSIZE];
  QString m_user_defined;

  // Decoded-callsign overlay state. List grows on each
  // addDecodeLabel(); ageDecodeLabels() runs from m_ageTimer and
  // prunes entries older than m_decodeLabelPeriods × m_TRperiod sec.
  // Capped at kDecodeLabelMax to protect paint loop on busy bands.
  QList<DecodeLabel> m_decodeLabels;
  QString m_activeCallsign;       // mirror of WSJT-X DX Call entry
  bool   m_decodeLabelsEnabled {true};
  int    m_decodeLabelPeriods  {1};   // default 1 = vanish after one TR period
  DecodeLabelFontSize m_decodeFontSize {DecodeLabelFontSize::Normal};
  // Overlay transparency preset (0..255). UI offers 255/200/125 mapped
  // to None/Medium/High in the View menu.
  int    m_decodeLabelAlpha    {255};
  QTimer m_ageTimer;
  static constexpr int kDecodeLabelMax = 300;

private slots:
  void ageDecodeLabels();
};

#endif // WIDEGRAPH_H
