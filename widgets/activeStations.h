// -*- Mode: C++ -*-
#ifndef ARRL_DIGI_H_
#define ARRL_DIGI_H_

#include <QWidget>
#include <QMap>
#include <QTimer>

class QSettings;
class QFont;

namespace Ui {
  class ActiveStations;
}

class ActiveStations
  : public QWidget
{
  Q_OBJECT

public:
  explicit ActiveStations(QSettings *, QFont const&, QWidget * parent = 0);
  ~ActiveStations();
  void displayRecentStations(QString mode, QString const&);
  void setupUi(QString display_mode);
  void changeFont (QFont const&);
  int  maxRecent();
  int  maxAge();
  void setClickOK(bool b);
  void erase();
  bool readyOnly();
  bool wantedOnly();
  void setRate(int n);
  void setBandChanges(int n);
  void setScore(int n);
  void clearStations();
  void addLine(QString);

  bool m_clickOK=false;
  bool m_bReadyOnly;
  bool m_bWantedOnly;

private:
  void read_settings ();
  void write_settings ();
  Q_SIGNAL void callSandP(int nline);
  Q_SIGNAL void activeStationsDisplay();
  Q_SIGNAL void cursorPositionChanged();
  Q_SIGNAL void queueActiveWindowHound(QString text);

  Q_SLOT void on_cbReadyOnly_toggled(bool b);
  Q_SLOT void on_cbWantedOnly_toggled(bool b);
  Q_SLOT void on_textEdit_clicked();

  QString m_mode="";
  QSettings * settings_;
  QString m_textbuffer="";                      // F/H mode band decodes
  QMap<int, QString> m_decodes_by_frequency;    // store decodes for F/H band awareness by frequency

  // Coalesces redraws: on a busy band addLine() fires many times per second;
  // doing a full rebuild + QTextEdit setPlainText() each call stalls the UI.
  // The map is updated synchronously so reads are fresh; the display refresh
  // is deferred to the next timer tick.
  QTimer m_refreshTimer;
  void flushDisplay();

  QScopedPointer<Ui::ActiveStations> ui;
};

#endif
