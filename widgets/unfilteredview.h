#ifndef UNFILTEREDVIEW_H
#define UNFILTEREDVIEW_H

#include <QFrame>

namespace Ui {
class UnfilteredView;
}

class UnfilteredView : public QFrame
{
    Q_OBJECT

public:
    explicit UnfilteredView(QWidget *parent = nullptr);
    ~UnfilteredView();
    void display(QString s);
    void setFont(QFont f);
    void erase() ;

private:
    Ui::UnfilteredView *ui;
};

#endif // UNFILTEREDVIEW_H
