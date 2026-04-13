#include "unfilteredview.h"
#include "ui_unfilteredview.h"

UnfilteredView::UnfilteredView(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::UnfilteredView)
{
    setWindowTitle ("WSJT-Z - Unfiltered View");
    ui->setupUi(this);
}

UnfilteredView::~UnfilteredView()
{
    delete ui;
}

void UnfilteredView::display (QString s)
{
    ui->rawView->append(s);
}

void UnfilteredView::setFont (QFont f) {
    ui->rawView->setFont(f);
}

void UnfilteredView::erase () {
    ui->rawView->clear();
}
