#include "widegraph.h"

#include <algorithm>
#include <QApplication>
#include <QSettings>
#include <QDateTime>
#include <QKeyEvent>
#include <math.h>
#include "ui_widegraph.h"
#include "commons.h"
#include "Configuration.hpp"
#include "MessageBox.hpp"
#include "SettingsGroup.hpp"
#include "moc_widegraph.cpp"

WideGraph::WideGraph(QSettings * settings, QWidget *parent) :
  QDialog(parent),
  ui(new Ui::WideGraph),
  m_settings (settings),
  m_palettes_path {":/Palettes"},
  m_tr0 {0.0},
  m_n {0},
  m_bHaveTransmitted {false},
  m_user_defined {tr ("User Defined")}
{
  ui->setupUi(this);

  setWindowTitle (QApplication::applicationName () + " - " + tr ("Wide Graph"));
  setWindowFlags (Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
  setMaximumWidth (MAX_SCREENSIZE);
  setMaximumHeight (880);

  ui->widePlot->setCursor(Qt::CrossCursor);
  ui->widePlot->setMaximumHeight(800);
  ui->widePlot->setCurrent(false);
  ui->cbControls->setCursor(Qt::ArrowCursor);
  ui->cbBars->setCursor(Qt::ArrowCursor);
  ui->cbClear->setCursor(Qt::ArrowCursor);
  ui->pbClear->setCursor(Qt::ArrowCursor);

  connect(ui->widePlot, SIGNAL(freezeDecode1(int)),this,
          SLOT(wideFreezeDecode(int)));

  connect(ui->widePlot, SIGNAL(setFreq1(int,int)),this,
          SLOT(setFreq2(int,int)));

  {
    //Restore user's settings
    SettingsGroup g {m_settings, "WideGraph"};
    restoreGeometry (m_settings->value ("geometry", saveGeometry ()).toByteArray ());
    ui->widePlot->setPlotZero(m_settings->value("PlotZero", 0).toInt());
    ui->widePlot->setPlotGain(m_settings->value("PlotGain", 0).toInt());
    ui->widePlot->setPlot2dGain(m_settings->value("Plot2dGain", 0).toInt());
    ui->widePlot->setPlot2dZero(m_settings->value("Plot2dZero", 0).toInt());
    ui->zeroSlider->setValue(ui->widePlot->plotZero());
    ui->gainSlider->setValue(ui->widePlot->plotGain());
    ui->gain2dSlider->setValue(ui->widePlot->plot2dGain());
    ui->zero2dSlider->setValue(ui->widePlot->plot2dZero());
    int n = m_settings->value("BinsPerPixel",2).toInt();
	m_bars=m_settings->value("Bars",true).toBool();
    ui->cbBars->setChecked(m_bars);
    ui->widePlot->setBars(m_bars);
    m_freq=m_settings->value("Freq",true).toBool();
    ui->cbFreq->setChecked(m_freq);
    ui->widePlot->showFreq(m_freq);
    m_clear=m_settings->value("Clear",true).toBool();
    ui->cbClear->setChecked(m_clear);
    ui->widePlot->setClear(m_clear);
    m_bFlatten=m_settings->value("Flatten",true).toBool();
    m_bRef=m_settings->value("UseRef",false).toBool();
    ui->cbFlatten->setChecked(m_bFlatten);
    ui->widePlot->setFlatten(m_bFlatten,m_bRef);
    ui->cbRef->setChecked(m_bRef);
    ui->widePlot->setBreadth(m_settings->value("PlotWidth",1000).toInt());
    ui->bppSpinBox->setValue(n);
    m_nsmo=m_settings->value("SmoothYellow",1).toInt();
    ui->smoSpinBox->setValue(m_nsmo);
    m_Percent2DScreen=m_settings->value("Percent2D",30).toInt();
    m_waterfallAvg = m_settings->value("WaterfallAvg",5).toInt();
    ui->waterfallAvgSpinBox->setValue(m_waterfallAvg);
    ui->widePlot->setWaterfallAvg(m_waterfallAvg);
    ui->widePlot->setCurrent(m_settings->value("Current",false).toBool());
    ui->widePlot->setCumulative(m_settings->value("Cumulative",true).toBool());
    ui->widePlot->setLinearAvg(m_settings->value("LinearAvg",false).toBool());
    ui->widePlot->setReference(m_settings->value("Reference",false).toBool());
    ui->widePlot->setQ65_Sync(m_settings->value("Q65_Sync",false).toBool());
    ui->widePlot->setTotalPower(m_settings->value("TotalPower",false).toBool());
    if(ui->widePlot->current()) ui->spec2dComboBox->setCurrentIndex(0);
    if(ui->widePlot->cumulative()) ui->spec2dComboBox->setCurrentIndex(1);
    if(ui->widePlot->linearAvg()) ui->spec2dComboBox->setCurrentIndex(2);
    if(ui->widePlot->Reference()) ui->spec2dComboBox->setCurrentIndex(3);
    if(ui->widePlot->Q65_Sync()) ui->spec2dComboBox->setCurrentIndex(4);
    if(ui->widePlot->TotalPower()) ui->spec2dComboBox->setCurrentIndex(5);
    int nbpp=m_settings->value("BinsPerPixel",2).toInt();
    ui->widePlot->setBinsPerPixel(nbpp);
    ui->sbPercent2dPlot->setValue(m_Percent2DScreen);
    ui->widePlot->setStartFreq(m_settings->value("StartFreq",0).toInt());
    ui->fStartSpinBox->setValue(ui->widePlot->startFreq());
    m_timestamp = 1; int itstamp=m_settings->value("Timestamp",1).toInt();
    QString ststamp=m_settings->value("Timestamp","1").toString();
    if(ststamp == "0" || ststamp == "1" || ststamp == "2") m_timestamp=itstamp;
    // Populate timestamp combo box before setting current index
    ui->timestampComboBox->addItem("Off");
    ui->timestampComboBox->addItem("UTC");
    ui->timestampComboBox->addItem("Local");
    ui->timestampComboBox->setCurrentIndex(m_timestamp); 
    ui->widePlot->setTimestamp(m_timestamp);
    m_waterfallPalette=m_settings->value("WaterfallPalette","Default").toString();
    m_userPalette = WFPalette {m_settings->value("UserPalette").value<WFPalette::Colours> ()};
    m_fMinPerBand = m_settings->value ("FminPerBand").toHash ();
    setRxRange ();
    ui->controls_widget->setVisible(!m_settings->value("HideControls",false).toBool());
    ui->cbControls->setChecked(!m_settings->value("HideControls",false).toBool());
  }

  int index=0;
  for (QString const& file:
         m_palettes_path.entryList(QDir::NoDotAndDotDot |
                                   QDir::System | QDir::Hidden |
                                   QDir::AllDirs | QDir::Files,
                                   QDir::DirsFirst)) {
    QString t=file.mid(0,file.length()-4);
    ui->paletteComboBox->addItem(t);
    if(t==m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
    index++;
  }
  ui->paletteComboBox->addItem (m_user_defined);
  if (m_user_defined == m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
  readPalette ();

  // Decoded-callsign overlay (N6NU 2026-05-11): load persisted toggle
  // + age-periods under [WideGraph] group. Defaults match QMAP-side
  // feature: ON, 1 TR period before vanish. Age timer at 1 Hz.
  if (m_settings) {
    SettingsGroup g {m_settings, "WideGraph"};
    m_decodeLabelsEnabled = m_settings->value("DecodeLabelsEnabled", true).toBool();
    m_decodeLabelPeriods = m_settings->value("DecodeLabelPeriods", 1).toInt();
    auto sz = m_settings->value("DecodeLabelFontSize", static_cast<int>(DecodeLabelFontSize::Normal)).toInt();
    m_decodeFontSize = static_cast<DecodeLabelFontSize>(sz);
    m_decodeLabelAlpha = m_settings->value("DecodeLabelAlpha", 255).toInt();
  }

  // WideGraph-side controls (mirror of QMAP's row): Show Callsigns
  // checkbox, periods spin, and Clear button. The View menu's existing
  // toggle / persist radios / etc. stay authoritative — these widgets
  // are just an in-window mirror so the operator doesn't have to open
  // the menu for the most-used knobs.
  if (ui) {
    ui->cbShowCallsigns->setChecked(m_decodeLabelsEnabled);
    ui->sbDecodeLabelPeriods->setMinimum(1);
    ui->sbDecodeLabelPeriods->setMaximum(5);
    ui->sbDecodeLabelPeriods->setValue(m_decodeLabelPeriods);
  }

  connect(&m_ageTimer, &QTimer::timeout, this, &WideGraph::ageDecodeLabels);
  m_ageTimer.start(1000);
}

WideGraph::~WideGraph ()
{
}

void WideGraph::closeEvent (QCloseEvent * e)
{
  saveSettings ();
  QDialog::closeEvent (e);
}

void WideGraph::saveSettings()                                           //saveSettings
{
  SettingsGroup g {m_settings, "WideGraph"};
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("PlotZero", ui->widePlot->plotZero());
  m_settings->setValue ("PlotGain", ui->widePlot->plotGain());
  m_settings->setValue ("Plot2dGain", ui->widePlot->plot2dGain());
  m_settings->setValue ("Plot2dZero", ui->widePlot->plot2dZero());
  m_settings->setValue ("PlotWidth", ui->widePlot->plotWidth ());
  m_settings->setValue ("BinsPerPixel", ui->bppSpinBox->value ());
  m_settings->setValue ("SmoothYellow", ui->smoSpinBox->value ());
  m_settings->setValue ("Percent2D",m_Percent2DScreen);
  m_settings->setValue ("WaterfallAvg", ui->waterfallAvgSpinBox->value ());
  m_settings->setValue ("Current", ui->widePlot->current());
  m_settings->setValue ("Cumulative", ui->widePlot->cumulative());
  m_settings->setValue ("LinearAvg", ui->widePlot->linearAvg());
  m_settings->setValue ("Reference", ui->widePlot->Reference());
  m_settings->setValue ("Q65_Sync", ui->widePlot->Q65_Sync());
  m_settings->setValue ("TotalPower", ui->widePlot->TotalPower());
  m_settings->setValue ("BinsPerPixel", ui->widePlot->binsPerPixel ());
  m_settings->setValue ("StartFreq", ui->widePlot->startFreq ());
  m_settings->setValue ("WaterfallPalette", m_waterfallPalette);
  m_settings->setValue ("UserPalette", QVariant::fromValue (m_userPalette.colours ()));
  m_settings->setValue("Flatten",m_bFlatten);
  m_settings->setValue("UseRef",m_bRef);
  m_settings->setValue ("HideControls", ui->controls_widget->isHidden ());
  m_settings->setValue ("Bars", m_bars);
  m_settings->setValue ("Freq", m_freq);
  m_settings->setValue ("Clear", m_clear);
  m_settings->setValue ("Timestamp", m_timestamp);
  m_settings->setValue ("FminPerBand", m_fMinPerBand);
  m_settings->setValue("DecodeLabelsEnabled", m_decodeLabelsEnabled);
  m_settings->setValue("DecodeLabelPeriods", m_decodeLabelPeriods);
  m_settings->setValue("DecodeLabelFontSize", static_cast<int>(m_decodeFontSize));
  m_settings->setValue("DecodeLabelAlpha", m_decodeLabelAlpha);
}

void WideGraph::drawRed(int ia, int ib)
{
  ui->widePlot->drawRed(ia,ib,m_swide);
}

void WideGraph::dataSink2(float s[], float df3, int ihsym, int ndiskdata, float pdB)  //dataSink2
{
  static float splot[NSMAX];
  int nbpp = ui->widePlot->binsPerPixel();

  if(ui->widePlot->TotalPower()) ui->widePlot->drawTotalPower(pdB);
//Average spectra over specified number, m_waterfallAvg
  if (m_n==0) {
    for (int i=0; i<NSMAX; i++)
      splot[i]=s[i];
  } else {
    for (int i=0; i<NSMAX; i++)
      splot[i] += s[i];
  }
  m_n++;

  if (m_n>=m_waterfallAvg) {
    for (int i=0; i<NSMAX; i++)
        splot[i] /= m_n;        //Normalize the average
    m_n=0;
    int i=int(ui->widePlot->startFreq()/df3 + 0.5);
    int jz=5000.0/(nbpp*df3);
		if(jz>MAX_SCREENSIZE) jz=MAX_SCREENSIZE;
    m_jz=jz;
    for (int j=0; j<jz; j++) {
      float ss=0.0;
      float smax=0;
      for (int k=0; k<nbpp; k++) {
        float sp=splot[i++];
        ss += sp;
        smax=qMax(smax,sp);
      }
//      m_swide[j]=nbpp*smax;
      m_swide[j]=nbpp*ss;
    }

// Time according to this computer
    qint64 ms = QDateTime::currentMSecsSinceEpoch() % 86400000;
    double tr = fmod(0.001*ms,m_TRperiod);
    if((ndiskdata && ihsym <= m_waterfallAvg) || (!ndiskdata && (tr<m_tr0))) {
      float flagValue=1.0e30;
      if(m_bHaveTransmitted) flagValue=2.0e30;
      for(int i=0; i<MAX_SCREENSIZE; i++) {
        m_swide[i] = flagValue;
      }
      for(int i=0; i<NSMAX; i++) {
        splot[i] = flagValue;
      }
      m_bHaveTransmitted=false;
    }
    m_tr0=tr;
    ui->widePlot->draw(m_swide,true,false);
  }
}

void WideGraph::on_bppSpinBox_valueChanged(int n)                            //bpp
{
  ui->widePlot->setBinsPerPixel(n);
}

void WideGraph::on_waterfallAvgSpinBox_valueChanged(int n)                  //Navg
{
  m_waterfallAvg = n;
  ui->widePlot->setWaterfallAvg(n);
}

void WideGraph::keyPressEvent(QKeyEvent *e)                                 //F11, F12
{  
  switch(e->key())
  {
  int n;
  case Qt::Key_F11:
    n=11;
    if(e->modifiers() & Qt::ControlModifier) n+=100;
    emit f11f12(n);
    break;
  case Qt::Key_F12:
    n=12;
    if(e->modifiers() & Qt::ControlModifier) n+=100;
    emit f11f12(n);
    break;
  default:
    QDialog::keyPressEvent (e);
  }
}

void WideGraph::setRxFreq(int n)                                           //setRxFreq
{
  ui->widePlot->setRxFreq(n);
  if(m_mode!="Q65") ui->widePlot->draw(m_swide,false,false);
}

int WideGraph::rxFreq()                                                   //rxFreq
{
  return ui->widePlot->rxFreq();
}

int WideGraph::nStartFreq()                                             //nStartFreq
{
  return ui->widePlot->startFreq();
}

void WideGraph::wideFreezeDecode(int n)                              //wideFreezeDecode
{
  emit freezeDecode2(n);
}

void WideGraph::setRxRange ()
{
  ui->widePlot->setRxRange (Fmin ());
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

int WideGraph::Fmin()                                              //Fmin
{
  return "60m" == m_rxBand ? 0 : m_fMinPerBand.value (m_rxBand, 2500).toUInt ();
}

int WideGraph::Fmax()                                              //Fmax
{
  return std::min(5000,ui->widePlot->Fmax());
}

int WideGraph::fSpan()
{
  return ui->widePlot->fSpan ();
}

void WideGraph::setPeriod(double trperiod, int nsps)                  //SetPeriod
{
  m_TRperiod=trperiod;
  m_nsps=nsps;
  ui->widePlot->setNsps(trperiod, nsps);
}

void WideGraph::setTxFreq(int n)                                   //setTxFreq
{
  emit setXIT2(n);
  ui->widePlot->setTxFreq(n);
}

void WideGraph::setMode(QString mode)                              //setMode
{
  m_mode=mode;
  ui->fSplitSpinBox->setEnabled(m_mode.startsWith("FST4"));
  ui->widePlot->setMode(mode);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

void WideGraph::setSubMode(int n)                                  //setSubMode
{
  m_nSubMode=n;
  ui->widePlot->setSubMode(n);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

void WideGraph::on_spec2dComboBox_currentIndexChanged(int index)
{
  ui->widePlot->setCurrent(false);
  ui->widePlot->setCumulative(false);
  ui->widePlot->setLinearAvg(false);
  ui->widePlot->setReference(false);
  ui->widePlot->setQ65_Sync(false);
  ui->widePlot->setTotalPower(false);
  ui->smoSpinBox->setEnabled(false);
  switch (index)
    {
    case 0:                     // Current
      ui->widePlot->setCurrent(true);
      break;
    case 1:                     // Cumulative
      ui->widePlot->setCumulative(true);
      break;
    case 2:                     // Linear Avg
      ui->widePlot->setLinearAvg(true);
      ui->smoSpinBox->setEnabled(true);
      break;
    case 3:                     // Reference
      ui->widePlot->setReference(true);
      break;
    case 4:
      ui->widePlot->setQ65_Sync(true);
      break;
    case 5:
      ui->widePlot->setTotalPower(true);
      break;
    }
  replot();
}

void WideGraph::on_fSplitSpinBox_valueChanged(int n)              //fSplit
{
  if (m_rxBand != "60m") m_fMinPerBand[m_rxBand] = n;
  setRxRange ();
}

void WideGraph::setFreq2(int rxFreq, int txFreq)                  //setFreq2
{
  emit setFreq3(rxFreq,txFreq);
}

void WideGraph::setDialFreq(double d)                             //setDialFreq
{
  ui->widePlot->setDialFreq(d);
}

void WideGraph::setRxBand (QString const& band)
{
  m_rxBand = band;
  if ("60m" == m_rxBand)
    {
      ui->fSplitSpinBox->setEnabled (false);
      ui->fSplitSpinBox->setValue (0);
    }
  else
    {
      ui->fSplitSpinBox->setValue (m_fMinPerBand.value (band, 2500).toUInt ());
      ui->fSplitSpinBox->setEnabled (m_mode.startsWith("FST4"));
    }
  ui->widePlot->setRxBand(band);
  setRxRange ();
}


void WideGraph::on_fStartSpinBox_valueChanged(int n)             //fStart
{
  ui->widePlot->setStartFreq(n);
}

void WideGraph::readPalette ()                                   //readPalette
{
  try
    {
      if (m_user_defined == m_waterfallPalette)
        {
          ui->widePlot->setColours (WFPalette {m_userPalette}.interpolate ());
        }
      else
        {
          ui->widePlot->setColours (WFPalette {m_palettes_path.absoluteFilePath (m_waterfallPalette + ".pal")}.interpolate());
        }
    }
  catch (std::exception const& e)
    {
      MessageBox::warning_message (this, tr ("Read Palette"), e.what ());
    }
}

void WideGraph::on_paletteComboBox_activated (QString const& palette)    //palette selector
{
  m_waterfallPalette = palette;
  readPalette();
  replot();
}

void WideGraph::on_cbFlatten_toggled(bool b)                          //Flatten On/Off
{
  m_bFlatten=b;
  if(m_bRef and m_bFlatten) {
    m_bRef=false;
    ui->cbRef->setChecked(false);
  }
  ui->widePlot->setFlatten(m_bFlatten,m_bRef);
}

void WideGraph::on_cbRef_toggled(bool b)
{
  m_bRef=b;
  if(m_bRef and m_bFlatten) {
    m_bFlatten=false;
    ui->cbFlatten->setChecked(false);
  }
  ui->widePlot->setFlatten(m_bFlatten,m_bRef);
}

void WideGraph::on_cbControls_toggled(bool b)
{
  ui->controls_widget->setVisible(b);
}

void WideGraph::on_cbBars_toggled(bool b)
{
  m_bars = b;
  ui->widePlot->setBars(m_bars);
}

void WideGraph::on_cbFreq_toggled(bool b)
{
  m_freq = b;
  ui->widePlot->showFreq(m_freq);
}

void WideGraph::on_cbClear_toggled(bool b)
{
  m_clear = b;
  ui->widePlot->setClear(m_clear);
}

void WideGraph::on_pbClear_clicked()
{
  ui->widePlot->update();
}

void WideGraph::on_timestampComboBox_currentIndexChanged(int n)
{
  m_timestamp = n;
  ui->widePlot->setTimestamp(m_timestamp);
}

void WideGraph::on_adjust_palette_push_button_clicked (bool)   //Adjust Palette
{
  try
    {
      if (m_userPalette.design ())
        {
          m_waterfallPalette = m_user_defined;
          ui->paletteComboBox->setCurrentText (m_waterfallPalette);
          readPalette ();
        }
    }
  catch (std::exception const& e)
    {
      MessageBox::warning_message (this, tr ("Read Palette"), e.what ());
    }
}

bool WideGraph::flatten()                                              //Flatten
{
  return m_bFlatten;
}

bool WideGraph::useRef()                                              //Flatten
{
  return m_bRef;
}

void WideGraph::replot()
{
  if(ui->widePlot->scaleOK()) ui->widePlot->replot();
}

void WideGraph::on_gainSlider_valueChanged(int value)                 //Gain
{
  ui->widePlot->setPlotGain(value);
  replot();
}

void WideGraph::on_zeroSlider_valueChanged(int value)                 //Zero
{
  ui->widePlot->setPlotZero(value);
  replot();
}

void WideGraph::on_gain2dSlider_valueChanged(int value)               //Gain2
{
  ui->widePlot->setPlot2dGain(value);
  if(ui->widePlot->TotalPower()) return;
  if(ui->widePlot->scaleOK ()) {
    ui->widePlot->draw(m_swide,false,false);
    if(m_mode=="Q65") ui->widePlot->draw(m_swide,false,true);
  }
}

void WideGraph::on_zero2dSlider_valueChanged(int value)               //Zero2
{
  ui->widePlot->setPlot2dZero(value);
  if(ui->widePlot->TotalPower()) return;
  if(ui->widePlot->scaleOK ()) {
    ui->widePlot->draw(m_swide,false,false);
    if(m_mode=="Q65") ui->widePlot->draw(m_swide,false,true);
  }
}

void WideGraph::setSuperFox(bool b)
{
  ui->widePlot->setSuperFox(b);
}

void WideGraph::setSuperHound(bool b)
{
  ui->widePlot->setSuperHound(b);
}

void WideGraph::setTol(int n)                                         //setTol
{
  ui->widePlot->setTol(n);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

void WideGraph::setFST4_FreqRange(int fLow,int fHigh)
{
  ui->widePlot->setFST4_FreqRange(fLow,fHigh);
}

void WideGraph::setSingleDecode(bool b)
{
  ui->widePlot->setSingleDecode(b);
}

void WideGraph::on_smoSpinBox_valueChanged(int n)
{
  m_nsmo=n;
}

int WideGraph::smoothYellow()
{
  return m_nsmo;
}

void WideGraph::setWSPRtransmitted()
{
  m_bHaveTransmitted=true;
}

void WideGraph::setVHF(bool bVHF)
{
  ui->widePlot->setVHF(bVHF);
}

void WideGraph::on_sbPercent2dPlot_valueChanged(int n)
{
  m_Percent2DScreen=n;
  ui->widePlot->SetPercent2DScreen(n);
}

void WideGraph::setRedFile(QString fRed)
{
  ui->widePlot->setRedFile(fRed);
}

void WideGraph::setDiskUTC(int nutc)
{
  ui->widePlot->setDiskUTC(nutc);
}

void WideGraph::restartTotalPower()
{
  ui->widePlot->restartTotalPower();
}

void WideGraph::addDecodeLabel(double freq_hz, const QString& callsign,
                               bool is_cq, int time_sec)
{
  if (callsign.isEmpty()) return;
  if (!m_decodeLabelsEnabled) return;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  // Period parity: decode time-of-day / TR period → period number.
  // Even = period 0/2/4..., odd = period 1/3/5... within the minute.
  const double trp = (m_TRperiod > 0) ? m_TRperiod : 15.0;
  const int    trp_sec = std::max(1, static_cast<int>(trp + 0.5));
  const bool   is_even = ((time_sec / trp_sec) % 2) == 0;
  const bool   is_active = !m_activeCallsign.isEmpty()
                           && callsign.compare(m_activeCallsign, Qt::CaseInsensitive) == 0;

  // Dedupe by callsign: refresh existing entry's timestamp + freq +
  // period parity (a refreshed entry takes the current decode's period).
  for (auto& lab : m_decodeLabels) {
    if (lab.callsign.compare(callsign, Qt::CaseInsensitive) == 0) {
      lab.freq_hz = freq_hz;
      lab.last_seen_ms = now;
      lab.is_even_period = is_even;
      lab.is_active = is_active;
      return;
    }
  }
  if (m_decodeLabels.size() >= kDecodeLabelMax) return;
  DecodeLabel lab{freq_hz, callsign, now, is_cq, is_even};
  lab.is_active = is_active;
  m_decodeLabels.append(lab);
  if (ui && ui->widePlot) ui->widePlot->setDecodeLabels(m_decodeLabels);
}

void WideGraph::clearDecodeLabels()
{
  if (m_decodeLabels.isEmpty()) return;
  m_decodeLabels.clear();
  if (ui && ui->widePlot) ui->widePlot->setDecodeLabels(m_decodeLabels);
}

void WideGraph::setActiveCallsign(const QString& call)
{
  if (call == m_activeCallsign) return;
  m_activeCallsign = call;
  // Re-flag each label's is_active. Cheap O(N) scan; the list is
  // bounded at kDecodeLabelMax (300).
  for (auto& lab : m_decodeLabels) {
    lab.is_active = !call.isEmpty()
                    && call.compare(lab.callsign, Qt::CaseInsensitive) == 0;
  }
  if (ui && ui->widePlot) ui->widePlot->setDecodeLabels(m_decodeLabels);
}

void WideGraph::ageDecodeLabels()
{
  if (m_decodeLabels.isEmpty()) return;
  // Use TR period (s) × user's periods count as the time-to-live.
  // 1 TR period = the natural cadence; user can extend for persistence.
  const double trp = (m_TRperiod > 0) ? m_TRperiod : 15.0;
  const qint64 ttl_ms = static_cast<qint64>(trp * m_decodeLabelPeriods * 1000.0);
  const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - ttl_ms;
  const int    before = m_decodeLabels.size();
  m_decodeLabels.erase(
      std::remove_if(m_decodeLabels.begin(), m_decodeLabels.end(),
                     [cutoff](const DecodeLabel& l) { return l.last_seen_ms < cutoff; }),
      m_decodeLabels.end());
  if (m_decodeLabels.size() != before && ui && ui->widePlot) {
    ui->widePlot->setDecodeLabels(m_decodeLabels);
  }
}

void WideGraph::setDecodeLabelsEnabled(bool on)
{
  if (m_decodeLabelsEnabled == on) return;
  m_decodeLabelsEnabled = on;
  if (m_settings) {
    SettingsGroup g {m_settings, "WideGraph"};
    m_settings->setValue("DecodeLabelsEnabled", on);
  }
  if (!on) {
    clearDecodeLabels();
  }
  emit decodeLabelsEnabledChanged(on);
}

void WideGraph::setDecodeLabelPeriods(int n)
{
  if (n < 1) n = 1;
  if (n > 5) n = 5;
  if (m_decodeLabelPeriods == n) return;
  m_decodeLabelPeriods = n;
  if (m_settings) {
    SettingsGroup g {m_settings, "WideGraph"};
    m_settings->setValue("DecodeLabelPeriods", n);
  }
  emit decodeLabelPeriodsChanged(n);
}

void WideGraph::on_cbShowCallsigns_toggled(bool b)
{
  setDecodeLabelsEnabled(b);
  if (ui) ui->sbDecodeLabelPeriods->setEnabled(b);
}

void WideGraph::on_sbDecodeLabelPeriods_valueChanged(int n)
{
  setDecodeLabelPeriods(n);
}

void WideGraph::on_pbClearDecodeLabels_clicked()
{
  clearDecodeLabels();
}
