#include "dialog.h"
#include <QtWidgets>
#include <QtCharts>

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
{
    QVBoxLayout* pMainLayout = new QVBoxLayout();
    this->setLayout(pMainLayout);

    // file
    QHBoxLayout* pFileLayout = new QHBoxLayout();
    pMainLayout->addLayout(pFileLayout);

    m_pDateFile = new QLineEdit(this);
    pFileLayout->addWidget(m_pDateFile);

    QPushButton* pOpenBtn = new QPushButton(tr("选择数据文件"), this);
    pFileLayout->addWidget(pOpenBtn);
    connect(pOpenBtn, SIGNAL(clicked(bool)), this, SLOT(openDataFile()));

    // progress
    QHBoxLayout* pProgressLayout = new QHBoxLayout();
    pMainLayout->addLayout(pProgressLayout);

    QLabel* pLable1 = new QLabel(tr("读取数据"));
    pProgressLayout->addWidget(pLable1, 1);
    QLabel* pLable2 = new QLabel(tr("数据去重"));
    pProgressLayout->addWidget(pLable2, 1);
    QLabel* pLable3 = new QLabel(tr("数据拆分"));
    pProgressLayout->addWidget(pLable3, 1);
    QLabel* pLable4 = new QLabel(tr("训练集优化"));
    pProgressLayout->addWidget(pLable4, 1);
    QLabel* pLable5 = new QLabel(tr("完成"));
    pProgressLayout->addWidget(pLable5, 1, Qt::AlignRight);

    m_pProgressBar = new QProgressBar(this);
    pMainLayout->addWidget(m_pProgressBar);
    m_pProgressBar->setTextVisible(false);
    m_pProgressBar->setMinimum(0);
    m_pProgressBar->setMaximum(500);

    // message and roc
    QHBoxLayout* pRocLayout = new QHBoxLayout();
    pMainLayout->addLayout(pRocLayout);

    m_pLogUI = new QTextEdit(this);
    pRocLayout->addWidget(m_pLogUI);
    m_pLogUI->setReadOnly(true);
//    m_pLogUI->setFixedHeight(150);

    QChart* m_pDataC = new QChart();
    QChartView* pDataCV = new QChartView(m_pDataC, this);
    pRocLayout->addWidget(pDataCV);
//    pDataCV->setFixedSize(150, 150);

    m_pDataC->setTitle(tr("数据分布"));
    m_pDataC->setTheme(QChart::ChartThemeDark);
    m_pDataC->setAnimationOptions(QChart::AllAnimations);
    m_pDataC->legend()->hide();

    QPieSeries* m_pDataS = new QPieSeries();
    m_pDataC->addSeries(m_pDataS);
    m_pDataS->setHoleSize(0.35);

    m_testD = m_pDataS->append(tr("测试集 100"), 100);
    m_testD->setLabelVisible();
    m_tranD = m_pDataS->append(tr("训练集 210"), 210);
    m_tranD->setLabelVisible();
    m_errorD = m_pDataS->append(tr("错误数据 10"), 10);
    m_errorD->setLabelVisible();
    m_doubleD = m_pDataS->append(tr("重复数据 20"), 20);
    m_doubleD->setLabelVisible();

    QChart* m_pRocC = new QChart();
    QChartView* pRocV = new QChartView(m_pRocC, this);
    pRocLayout->addWidget(pRocV);

    m_pRocC->setTitle("ROC");
    m_pRocC->createDefaultAxes();
    m_pRocC->setTheme(QChart::ChartThemeDark);
    m_pRocC->setAnimationOptions(QChart::AllAnimations);
    m_pRocC->legend()->hide();

    QLineSeries* m_rocL = new QLineSeries();
    m_rocL->append(0, 0);
    m_rocL->append(1, 1);
    m_pRocC->addSeries(m_rocL);
    m_pRocC->createDefaultAxes();

    // btn
    QHBoxLayout* pBtnLayout = new QHBoxLayout();
    pMainLayout->addLayout(pBtnLayout);

    QPushButton* pSaveBtn = new QPushButton(tr("保存模型"));
    pBtnLayout->addStretch();
    pBtnLayout->addWidget(pSaveBtn);
    connect(pSaveBtn, SIGNAL(clicked(bool)), this, SLOT(saveModel()));

    QPushButton* pDumpBtn = new QPushButton(tr("导出数据"));
    pBtnLayout->addWidget(pDumpBtn);
    connect(pDumpBtn, SIGNAL(clicked(bool)), this, SLOT(dumpData()));

    QGroupBox* pUserBox = new QGroupBox(tr("平均值调整(未作)"), this);
    pMainLayout->addWidget(pUserBox);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QRgb(0x40434a));
    pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
    setPalette(pal);

    setMinimumSize(900, 400);
}

Dialog::~Dialog()
{

}

void Dialog::openDataFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("选择数据文件"), "", tr("Excel (*.xls *.xlsx)"));
    m_pDateFile->setText(filename);

    m_pProgressBar->setValue(1);


}

void Dialog::saveModel()
{

}

void Dialog::dumpData()
{

}
