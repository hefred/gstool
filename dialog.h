#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QtCharts/QChartGlobal>


QT_BEGIN_NAMESPACE
class QLineEdit;
class QProgressBar;
class QTextEdit;
QT_END_NAMESPACE

QT_CHARTS_BEGIN_NAMESPACE
class QPieSlice;
class QLineSeries;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class Dialog : public QDialog
{
    Q_OBJECT

public:
    Dialog(QWidget *parent = 0);
    ~Dialog();

protected slots:
    void openDataFile();
    void saveModel();
    void dumpData();

private:
    QLineEdit* m_pDateFile;
    QProgressBar* m_pProgressBar;
    QTextEdit* m_pLogUI;

    QPieSlice* m_testD;
    QPieSlice* m_tranD;
    QPieSlice* m_errorD;
    QPieSlice* m_doubleD;

    QLineSeries* m_rocL;
};

#endif // DIALOG_H
