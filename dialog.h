#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QtCharts/QChartGlobal>

class ExcelRW;
class GaussianModel;

QT_BEGIN_NAMESPACE
class QLineEdit;
class QProgressBar;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QButtonGroup;
QT_END_NAMESPACE

QT_CHARTS_BEGIN_NAMESPACE
class QPieSlice;
class QLineSeries;
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class SDataStruct : public QObject
{
    Q_OBJECT

public:
    SDataStruct(QObject *parent = Q_NULLPTR);
    ~SDataStruct();
    void init();

signals:
    void logMsg(QString msg);

public:

    int irow;
    int icol;
    int iprogress;
    bool bfinished;
    QString msg;

    QStringList xlsTitles;

    QHash<QString, int> itemKeys;
    QVector<QVector<double> > validDatas;

    QVector<QVector<double> > trainDatas;
    QVector<QVector<double> > testDatas;

    QVector<QVector<double> > badDatas;
    QList<QVariant> errorDatas;
    QVector<QVector<double> > repeatDatas;

    GaussianModel* model;
    int numRocLines;
    int bestRocLine;
    QVector<QList<QPointF>> rocLines;
    QVector<double> rocArea;

    QVector<double> bestPredictValues;
};

typedef QVector<double>     DblVec;
typedef QVector<DblVec>     DblVecVec;

class Dialog : public QDialog
{
    Q_OBJECT

public:
    Dialog(QWidget *parent = 0);
    ~Dialog();

    static QString parseDate(QList<QList<QVariant> > &m_xlsDatas, SDataStruct *datas);
    static QString splitDate(SDataStruct *datas);
    static QString trainModel(SDataStruct *datas);

protected slots:
    void openDataFile();
    void saveModel();
    void dumpData();

protected:
    static void run();
    void timerEvent(QTimerEvent *event);
private:
    int m_timerId;
    QLineEdit* m_pDateFile;

    QProgressBar* m_pProgressBar;
    QTextEdit* m_pLogUI;

    QPieSlice* m_badD;
    QPieSlice* m_testD;
    QPieSlice* m_tranD;
    QPieSlice* m_errorD;
    QPieSlice* m_repeatD;
    QChart* m_pRocC;
    SDataStruct m_data;

    QList< QList<QVariant> > m_xlsDatas;
};

#endif // DIALOG_H
