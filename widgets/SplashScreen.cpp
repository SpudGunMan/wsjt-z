#include "SplashScreen.hpp"

#include <QPixmap>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>

#include "revision_utils.hpp"
#include "pimpl_impl.hpp"

#include "moc_SplashScreen.cpp"

class SplashScreen::impl
{
public:
  impl ()
    : checkbox_ {"Do not show this again"}
  {
    main_layout_.addStretch ();
    main_layout_.addWidget (&checkbox_, 0, Qt::AlignLeft);
  }

  QVBoxLayout main_layout_;
  QCheckBox checkbox_;
};

SplashScreen::SplashScreen ()
  : QSplashScreen {QPixmap {":/splash.png"}, Qt::WindowStaysOnTopHint}
{
  setLayout (&m_->main_layout_);
  showMessage (
	"<p style='color:red; background-color: white;'><b>For support / bug reports / questions please visit our forum here:<br /> https://groups.io/g/WSJT-Z/</b></p>"
	"<hr>"
	"<p style='color:red; background-color: white;'><h1>If you like the software, please donate to our charity project. Details on the forum. </h1></p>"

    );
  connect (&m_->checkbox_, &QCheckBox::stateChanged, [this] (int s) {
      if (Qt::Checked == s) Q_EMIT disabled ();
    });
}

SplashScreen::~SplashScreen ()
{
}
