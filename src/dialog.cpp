#if _MSC_VER >= 1600
#pragma execution_character_set("UTF-8")
#endif

#include "dialog.h"
#include <QtWidgets>
#include <QtCharts>
#include <QtConcurrent>
#include "ExcelRW.h"
#include "gaussianmodel.h"

SDataStruct::SDataStruct(QObject *parent)
{
    irow=0;
    icol=0;
    iprogress=0;
    haveRepeatFlag = false;
    bfinished=false;
    bStopTraining = false;

    model = NULL;
    numRocLines = 0;
    currentRocLine = 0;
    bestPredictIndex = 0;
}

SDataStruct::~SDataStruct()
{
    if(model)
        delete model;
}

void SDataStruct::init()
{
    irow=0;
    icol=0;
    iprogress=0;
    haveRepeatFlag = false;
    bfinished=false;
    bStopTraining = false;

    xlsTitles.clear();

    itemKeys.clear();
    validDatas.clear();

    trainDatas.clear();
    testDatas.clear();

    badDatas.clear();
    errorDatas.clear();
    repeatDatas.clear();

    if(model)
        delete model;
    model = new GaussianModel();

    numRocLines = 0;
    currentRocLine = 0;
    bestPredictIndex = 0;
    rocLines.clear();
    rocArea.clear();
    predictValues.clear();
}


Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
{
    m_timerId = 0;

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
    QLabel* pLable2 = new QLabel(tr("转换去重"));
    pProgressLayout->addWidget(pLable2, 1);
    QLabel* pLable3 = new QLabel(tr("拆分测试集"));
    pProgressLayout->addWidget(pLable3, 1);
    QLabel* pLable4 = new QLabel(tr("循环优化"));
    pProgressLayout->addWidget(pLable4, 1);
    QLabel* pLable5 = new QLabel(tr("完成"));
    pProgressLayout->addWidget(pLable5, 1, Qt::AlignRight);

    m_pProgressBar = new QProgressBar(this);
    pMainLayout->addWidget(m_pProgressBar);
    m_pProgressBar->setTextVisible(false);
    m_pProgressBar->setMinimum(0);
    m_pProgressBar->setMaximum(500);

    // ROC area
    m_pRocAreaC = new QChart();
    QChartView* pRocAreaV = new QChartView(m_pRocAreaC, this);
    pMainLayout->addWidget(pRocAreaV);
    m_pRocAreaC->setTheme(QChart::ChartThemeDark);
    m_pRocAreaC->setAnimationOptions(QChart::NoAnimation);
    m_pRocAreaC->legend()->hide();
    pRocAreaV->setRenderHint(QPainter::Antialiasing, true);

    m_rocAreaL = new QLineSeries();
    m_pRocAreaC->addSeries(m_rocAreaL);
    m_rocAreaL->setPointsVisible();
    m_pRocAreaC->createDefaultAxes();
    m_pRocAreaC->axisY()->setRange(0.5, 1);

    // message and roc
    QHBoxLayout* pRocLayout = new QHBoxLayout();
    pMainLayout->addLayout(pRocLayout);

    m_pLogUI = new QTextEdit(this);
    pRocLayout->addWidget(m_pLogUI);
    m_pLogUI->setReadOnly(true);
    m_pLogUI->setWordWrapMode(QTextOption::NoWrap);

    QChart* m_pDataC = new QChart();
    QChartView* pDataCV = new QChartView(m_pDataC, this);
    pRocLayout->addWidget(pDataCV);
    pDataCV->setRenderHint(QPainter::Antialiasing, true);

    m_pDataC->setTitle(tr("数据分布"));
    m_pDataC->setTheme(QChart::ChartThemeDark);
    m_pDataC->setAnimationOptions(QChart::NoAnimation);
    m_pDataC->legend()->hide();

    QPieSeries* m_pDataS = new QPieSeries();
    m_pDataC->addSeries(m_pDataS);
    m_pDataS->setPieSize(0.5);
    m_pDataS->setHoleSize(0.25);

    m_tranD = m_pDataS->append(tr("训练集 0"), 1);
    m_badD = m_pDataS->append(tr("剔除 0"), 1);
    m_testD = m_pDataS->append(tr("测试集 0"), 1);
    m_repeatD = m_pDataS->append(tr("重复 0"), 1);
    m_errorD = m_pDataS->append(tr("残缺 0"), 1);

    m_pDataS->setLabelsVisible();
//    m_pDataS->setLabelsPosition(QPieSlice::LabelInsideNormal);

    m_pRocC = new QChart();
    QChartView* pRocV = new QChartView(m_pRocC, this);
    pRocLayout->addWidget(pRocV);

    m_pRocC->setTitle("ROC");
    m_pRocC->createDefaultAxes();
    m_pRocC->setTheme(QChart::ChartThemeDark);
    m_pRocC->setAnimationOptions(QChart::SeriesAnimations);
    m_pRocC->legend()->hide();
    pRocV->setRenderHint(QPainter::Antialiasing, true);

    m_rocL = new QLineSeries();
    m_rocL->append(0, 0);
    m_rocL->append(1, 1);
    m_pRocC->addSeries(m_rocL);
    m_pRocC->createDefaultAxes();

    // btn
    QHBoxLayout* pBtnLayout = new QHBoxLayout();
    pMainLayout->addLayout(pBtnLayout);

    QPushButton* pStopBtn = new QPushButton(tr("暂停优化"));
    pBtnLayout->addWidget(pStopBtn);
    connect(pStopBtn, SIGNAL(clicked(bool)), this, SLOT(stopTraining()));

    QPushButton* pSaveBtn = new QPushButton(tr("保存模型"));
    pBtnLayout->addStretch();
    pBtnLayout->addWidget(pSaveBtn);
    connect(pSaveBtn, SIGNAL(clicked(bool)), this, SLOT(saveModel()));

    QPushButton* pDumpBtn = new QPushButton(tr("导出数据"));
    pBtnLayout->addWidget(pDumpBtn);
    connect(pDumpBtn, SIGNAL(clicked(bool)), this, SLOT(dumpData()));

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QRgb(0x40434a));
    pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
    setPalette(pal);

    connect(&m_data, SIGNAL(logMsg(QString)), m_pLogUI, SLOT(append(QString)));
    setMinimumSize(900, 700);
}

Dialog::~Dialog()
{

}

QString Dialog::parseDate(QList<QList<QVariant> > &xlsDatas, SDataStruct* datas)
{
    emit datas->logMsg(tr("\n*******************\nBegin parse data"));
    datas->validDatas.clear();
    bool bok = false;
    int numTarget = 1;
    if (datas->haveRepeatFlag)
        numTarget = 2;

    for (int i=1; i<datas->irow; i++)
    {
        QString itemKey;
        QList<QVariant> values = xlsDatas[i];
        QVector<double> rvs(datas->icol);
        bok = false;
        for (int j=0; j<datas->icol; j++)
        {
            double vlaue = values[j].toDouble(&bok);
            if (values[j].isNull() || !values[j].isValid() || !bok)
            {
                break;
            }
            else
            {
                rvs[j] = vlaue;
            }
            if (j >= numTarget)
            {
                itemKey += "_";
                itemKey += values[j].toString();
            }
        }

        if (bok)
        {
            if (!datas->itemKeys.contains(itemKey))
            {
                datas->itemKeys.insert(itemKey, datas->validDatas.size());
                datas->validDatas.append(rvs);
            }
            else
            {
                datas->repeatDatas.append(rvs);
                int iDelete = datas->itemKeys[itemKey];

                if (iDelete >= 0)
                {
                    QString movekey = datas->itemKeys.key(datas->validDatas.size()-1);
                    datas->itemKeys[movekey] = iDelete;

                    datas->validDatas[iDelete] = datas->validDatas.back();
                    datas->validDatas.resize(datas->validDatas.size()-1);

                    datas->itemKeys[itemKey] = -1;
                }
            }
        }
        else
        {
            // put to error value list
            datas->errorDatas.append(values);
        }

        datas->iprogress = 100 + qRound((98.0/datas->irow)*i);
        Sleep(10);
    }

    datas->iprogress = 200;

    // split data
    Dialog::splitDate(datas);

    Dialog::trainModel(datas);

    datas->bfinished = true;
    datas->msg = "";
    return "";
}

QString Dialog::splitDate(SDataStruct *datas)
{
    emit datas->logMsg(tr("\n*******************\nBegin split data"));
    int numValid = datas->validDatas.size();
    int numTran = numValid/4 * 3;
    if (numTran < 3)
    {
        datas->bfinished = true;
        datas->msg = tr("有效数据太少！");
        return datas->msg;
    }
    datas->iprogress += 5;

    for(int i=0; i < numValid; i++)
    {
        if (i%3 == 2)
            datas->testDatas.append(datas->validDatas[i]);
        else
            datas->trainDatas.append(datas->validDatas[i]);

        Sleep(1);
        datas->iprogress = 200 + qRound((90.0/numValid)*i);
    }

    datas->iprogress = 300;
    return "";
}

QString Dialog::trainModel(SDataStruct *datas)
{
    emit datas->logMsg(tr("\n*******************\nBegin training data"));
    int trainRow = datas->trainDatas.size();
    int predictRow = datas->testDatas.size();

    int targetId = 0;
    if (datas->haveRepeatFlag)
        targetId = 1;

    int col = datas->xlsTitles.size() - 1 - targetId;

    DblVecVec training_data;
    DblVecVec predict_data;
    DblVec training_meas;
    DblVec predict_meas;
    DblVec real_predict_meas;
    training_data.resize(trainRow);
    training_meas.resize(trainRow);
    predict_data.resize(predictRow);
    predict_meas.resize(predictRow);
    real_predict_meas.resize(predictRow);
    datas->iprogress = 305;

    for(int i = 0 ; i < trainRow; i++)
    {
        training_meas[i] = datas->trainDatas[i][targetId];
        training_data[i].resize(col);
        for (int j=0; j<col; j++)
        {
           training_data[i][j] = datas->trainDatas[i][targetId+j+1];
        }

        datas->iprogress = 305 + qRound((20.0/trainRow)*i);

        Sleep(1);
    }
    datas->iprogress = 330;

    for(int i = 0 ; i < predictRow; i++)
    {
        real_predict_meas[i] = datas->testDatas[i][0];

        predict_data[i].resize(col);
        for (int j=0; j<col; j++)
        {
           predict_data[i][j] = datas->testDatas[i][targetId+j+1];
        }
        datas->iprogress = 330 + qRound((10.0/predictRow)*i);

        Sleep(1);
    }
    datas->iprogress = 340;
    Sleep(10);

    double Max_area = 0;
    int bad_index = 0;
    DblVecVec new_trainingdata;
    DblVec new_trainingmeas;
    QList<double> thresholdList;
    QList<QPointF> bestRocLine;
    DblVec bestPredictMeas;
    QList<QPointF> rocLine;

    QString msg;
    int numLoop = trainRow - 3;
    double stepProgress = 150.0/double(numLoop);
    for(int time = 0; time < numLoop; time++)
    {
        msg = QString("\n############\n Begin %1 time training data for find out bad data").arg(time);
        emit datas->logMsg(msg);

        Max_area = 0;
        for(int index = 0; index < trainRow; index++)
        {
            // can stop the loop
            while (datas->bStopTraining)
            {
                Sleep(100);
            }

//            msg = QString("  Try delete #%1 train data").arg(index);
//            emit datas->logMsg(msg);

            new_trainingdata.clear();
            new_trainingmeas.clear();
            int trainSize=0;

            for(int index_new = 0; index_new < trainRow; index_new++)
            {
                if(index_new != index)
                {
                    new_trainingdata.push_back(training_data[index_new]);
                    new_trainingmeas.push_back(training_meas[index_new]);
                    trainSize++;
                }
            }

            //crate model
            datas->model->train(new_trainingdata, new_trainingmeas, trainSize, col);
            //predict
            datas->model->predict(predict_data, predictRow, predict_meas);

            /*       predict |  1   |   0   |
            -----------------------------------------
                       |  1  |  TP  |  FN   |  y = TPR=TP/(TP+FN)
                real   ------------------------------
                       |  0  |  FP  |  TN   |  x = FPR=FP/(FP+TN)
                       ------------------------------
            */
            thresholdList.clear();
            for(int i = 0; i < predictRow; i++)
                thresholdList.append(predict_meas[i]);
            qSort(thresholdList.begin(), thresholdList.end(), qGreater<double>());

            rocLine.clear();
            double area = 0, pastTPR = 0, pastFPR = 0;
            for(int j = 0; j < 10; j++)
            {
                double threshold = thresholdList[static_cast<int>(predictRow*0.1*j)];

                int TP = 0, FP = 0, TN = 0, FN = 0;
                int value = 0;
                for(int i = 0; i < predictRow; i++)
                {
                    if(predict_meas[i] < threshold)
                        value = 0;
                    else
                        value = 1;
                    if(fabs(real_predict_meas[i]) < 0.1)
                    {
                        if(value == 0)
                            TN++;
                        else
                            FP++;
                    }
                    else
                    {
                        if(value == 0)
                            FN++;
                        else
                            TP++;
                    }
                }
                double FPR = (FP*1.0)/(FP+TN);
                double TPR = (TP*1.0)/(TP+FN);

                rocLine.append(QPointF(FPR, TPR));

                area += (TPR + pastTPR)*(FPR-pastFPR)/2;
                pastTPR = TPR;
                pastFPR = FPR;
            }
            area += (1 + pastTPR)*(1-pastFPR)/2;
//            msg = tr("  Roc area %1 ").arg(area);
//            emit datas->logMsg(msg);

            if(area > Max_area)
            {
                Max_area = area;
                bad_index = index;

                bestRocLine = rocLine;
                bestPredictMeas = predict_meas;
            }

            Sleep(1);
        }

        datas->iprogress = 340 + qFloor(stepProgress*time);

        datas->predictValues.append(bestPredictMeas);
        datas->rocLines.append(bestRocLine);
        datas->rocArea.append(Max_area);

        qDebug() << real_predict_meas;
        qDebug() << bestPredictMeas;

        if (Max_area > datas->rocArea[datas->bestPredictIndex])
        {
            datas->bestPredictIndex = datas->numRocLines;
        }
        datas->numRocLines++;

        //handle for next round.
        msg = tr("  Best area %1, Find a bad train data %2, move it").arg(Max_area).arg(bad_index);
        emit datas->logMsg(msg);

        training_data.erase(training_data.begin()+bad_index);
        training_meas.erase(training_meas.begin()+bad_index);

        datas->badDatas.append(datas->trainDatas[bad_index]);
        datas->trainDatas.remove(bad_index);

        bad_index = 0;
        trainRow--;

        Sleep(1);
    }

    datas->iprogress = 500;
    datas->bfinished = true;
    return "";
}

void Dialog::openDataFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("选择数据文件"), "", tr("Excel (*.xls *.xlsx)"));
    if (filename.isEmpty())
        return;

    m_pDateFile->setText(filename);
    m_pProgressBar->setValue(1);

    ExcelRW xlsReader;
    xlsReader.open(filename);

    m_xlsDatas.clear();
    xlsReader.setCurrentSheet(1);
    xlsReader.readAll(m_xlsDatas);
    xlsReader.close();

    if (m_xlsDatas.size() < 3)
    {
        QMessageBox::information(this, "Error", "Too less data!");
        return;
    }
    m_pProgressBar->setValue(10);

    //init UI and data
    m_data.init();
    m_rocAreaL->clear();
//    m_pRocC->removeAllSeries();
    m_rocL->clear();
    m_rocL->append(0, 0);
    m_rocL->append(1, 1);
    m_pRocC->setTitle(tr("Roc AUC=0.5"));
    m_tranD->setValue(1);
    m_tranD->setLabel(tr("训练集 0"));
    m_testD->setValue(1);
    m_testD->setLabel(tr("训练集 0"));
    m_repeatD->setValue(1);
    m_repeatD->setLabel(tr("重复 0"));
    m_errorD->setValue(1);
    m_errorD->setLabel(tr("残缺 0"));
    m_badD->setValue(1);
    m_badD->setLabel(tr("剔除 0"));

    int irow = m_xlsDatas.size();
    int icol = m_xlsDatas[0].size();
    m_data.icol = icol;
    m_data.irow = irow;

    for (int k=0; k<icol; k++)
    {
        QString title = m_xlsDatas[0][k].toString();
        if (m_xlsDatas[0][k].isNull() || !m_xlsDatas[0][k].isValid() || title.isEmpty())
        {
            m_data.msg = QString("Title #%d is not valid!").arg(k);
            QMessageBox::information(this, "Error", m_data.msg);
            return;
        }
        m_data.xlsTitles.append(title);
    }
    m_pProgressBar->setValue(100);

    if (m_data.xlsTitles[1].contains("repeat", Qt::CaseInsensitive))
    {
        m_data.haveRepeatFlag = true;
    }

    m_data.iprogress = 100;
    QFuture<QString> future = QtConcurrent::run(Dialog::parseDate, m_xlsDatas, &m_data);
    m_timerId = startTimer(10);
}

void Dialog::saveModel()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("保存模型文件"), "gmodel.bin");

    QFile saveFile(filename);
    saveFile.open(QIODevice::ReadWrite);
    QDataStream out(&saveFile);

    out << m_data.model;

    saveFile.close();
    QMessageBox::information(this, "", tr("保存成功"));
}

void Dialog::dumpData()
{
    QString xlsFile = QFileDialog::getSaveFileName(this, tr("保存计算数据"), "model_data.xls", tr("Excel (*.xls *.xlsx)"));

    QElapsedTimer timer;
    timer.start();
    ExcelRW xlsWer;
    xlsWer.create(xlsFile);
    qDebug()<<"create cost:"<<timer.elapsed()<<"ms";timer.restart();
    QList<QList<QVariant> > sheetDatas;


    xlsWer.setName(1, tr("训练数据"));
    xlsWer.setName(2, tr("测试数据"));
    xlsWer.setName(3, tr("最坏数据"));
    xlsWer.setName(4, tr("重复数据"));
    xlsWer.setName(5, tr("无效数据"));

    // train data
    xlsWer.setCurrentSheet(1);
    QList<QVariant> tileRow;
    for (int i=0; i<m_data.icol; i++)
    {
        tileRow.append(m_data.xlsTitles[i]);
    }
    sheetDatas.append(tileRow);

    for (int i=0; i<m_data.trainDatas.size(); i++)
    {
        sheetDatas.append(QList<QVariant>());
        for (int j=0; j<m_data.icol; j++)
        {
            sheetDatas[i+1].append(m_data.trainDatas[i][j]);
        }
    }

    timer.restart();
    xlsWer.writeCurrentSheet(sheetDatas);
    qDebug()<<"write tran data cost:"<<timer.elapsed()<<"ms";timer.restart();

    // bad data
    xlsWer.setCurrentSheet(3);
    sheetDatas.clear();
    sheetDatas.append(tileRow);
    for (int i=0; i<m_data.badDatas.size(); i++)
    {
        sheetDatas.append(QList<QVariant>());
        for (int j=0; j<m_data.icol; j++)
        {
            sheetDatas[i+1].append(m_data.badDatas[i][j]);
        }
    }
    timer.restart();
    xlsWer.writeCurrentSheet(sheetDatas);
    qDebug()<<"write bad data cost:"<<timer.elapsed()<<"ms";timer.restart();

    // repeat data
    xlsWer.setCurrentSheet(4);
    sheetDatas.clear();
    sheetDatas.append(tileRow);
    for (int i=0; i<m_data.repeatDatas.size(); i++)
    {
        sheetDatas.append(QList<QVariant>());
        for (int j=0; j<m_data.icol; j++)
        {
            sheetDatas[i+1].append(m_data.repeatDatas[i][j]);
        }
    }
    timer.restart();
    xlsWer.writeCurrentSheet(sheetDatas);
    qDebug()<<"write repeat data cost:"<<timer.elapsed()<<"ms";timer.restart();

    // error data
    xlsWer.setCurrentSheet(5);
    sheetDatas.clear();
    sheetDatas.append(tileRow);
    for (int i=0; i<m_data.errorDatas.size(); i++)
    {
        sheetDatas.append(m_data.errorDatas[i]);
    }
    timer.restart();
    xlsWer.writeCurrentSheet(sheetDatas);
    qDebug()<<"write error data cost:"<<timer.elapsed()<<"ms";timer.restart();

    // test data
    xlsWer.setCurrentSheet(2);
    sheetDatas.clear();
    if (m_data.bfinished)
        tileRow << tr("最优预测值");
    else
        tileRow << tr("当前预测值");
    sheetDatas.append(tileRow);
    for (int i=0; i<m_data.testDatas.size(); i++)
    {
        sheetDatas.append(QList<QVariant>());
        for (int j=0; j<m_data.icol; j++)
        {
            sheetDatas[i+1].append(m_data.testDatas[i][j]);
        }
        if (m_data.bfinished)
            sheetDatas[i+1].append(m_data.predictValues[m_data.bestPredictIndex][i]);
        else
            sheetDatas[i+1].append(m_data.predictValues.back()[i]);
    }
    timer.restart();
    xlsWer.writeCurrentSheet(sheetDatas);
    qDebug()<<"write error data cost:"<<timer.elapsed()<<"ms";timer.restart();

    xlsWer.save();
    xlsWer.close();

    QMessageBox::information(this, "", tr("保存成功"));
}

void Dialog::stopTraining()
{
     if (m_data.bStopTraining)
     {
         m_data.bStopTraining = false;
         QPushButton* pStopBtn = dynamic_cast<QPushButton*>(sender());
         if (pStopBtn)
             pStopBtn->setText(tr("暂停优化"));

         if (m_timerId == 0)
            m_timerId = startTimer(10);
     }
     else
     {
         m_data.bStopTraining = true;
         QPushButton* pStopBtn =  dynamic_cast<QPushButton*>(sender());
         if (pStopBtn)
             pStopBtn->setText(tr("继续优化"));
     }
}

void Dialog::timerEvent(QTimerEvent *event)
{
    if (m_data.bfinished)
    {
        killTimer(m_timerId);
        m_timerId = 0;
        if (!m_data.msg.isEmpty())
        {
            QMessageBox::information(this, "Error", m_data.msg);
            return;
        }

        if (m_data.numRocLines > 0)
        {
            m_rocL->clear();
            m_rocL->append(m_data.rocLines[m_data.bestPredictIndex]);
            m_pRocC->setTitle(tr("Best Roc AUC=%1").arg(m_data.rocArea[m_data.bestPredictIndex]));
        }
    }

    if (m_data.trainDatas.size())
    {
        m_tranD->setValue(m_data.trainDatas.size());
        m_tranD->setLabel(tr("训练集 %1").arg(m_data.trainDatas.size()));
        m_testD->setValue(m_data.testDatas.size());
        m_testD->setLabel(tr("测试集 %1").arg(m_data.testDatas.size()));

        m_badD->setValue(m_data.badDatas.size());
        m_badD->setLabel(tr("剔除 %1").arg(m_data.badDatas.size()));
    }

    if (m_data.repeatDatas.size())
    {
        m_repeatD->setValue(m_data.repeatDatas.size());
        m_repeatD->setLabel(tr("重复 %1").arg(m_data.repeatDatas.size()));
        m_repeatD->setLabelVisible(true);
    }
    else
    {
        m_repeatD->setValue(0);
        m_repeatD->setLabelVisible(false);
    }

    if (m_data.errorDatas.size())
    {
        m_errorD->setValue(m_data.errorDatas.size());
        m_errorD->setLabel(tr("残缺 %1").arg(m_data.errorDatas.size()));
        m_errorD->setLabelVisible(true);
    }
    else
    {
        m_errorD->setValue(0);
        m_errorD->setLabelVisible(false);
    }

    if (m_data.currentRocLine < m_data.numRocLines)
    {
        m_pRocAreaC->axisX()->setRange(0, m_data.numRocLines);
        for (; m_data.currentRocLine < m_data.numRocLines; )
        {
            m_rocAreaL->append(m_data.currentRocLine, m_data.rocArea[m_data.currentRocLine]);
            m_data.currentRocLine++;
        }

        m_rocL->clear();
        m_rocL->append(m_data.rocLines.back());
        m_pRocC->setTitle(tr("Roc AUC=%1").arg(m_data.rocArea.back()));
        m_pRocC->update();
    }

    if (m_pLogUI->document()->lineCount() > 2000)
        m_pLogUI->clear();

    m_pProgressBar->setValue(m_data.iprogress);
}

