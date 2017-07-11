﻿#if _MSC_VER >= 1600
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
    bestRocLine = 0;
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
    bestRocLine = 0;
    rocLines.clear();
    rocArea.clear();
    currentPredictValues.clear();
    bestPredictValues.clear();
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
    QLabel* pLable4 = new QLabel(tr("10次循环优化训练"));
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
    m_pLogUI->setWordWrapMode(QTextOption::NoWrap);
//    m_pLogUI->setFixedHeight(150);

    QChart* m_pDataC = new QChart();
    QChartView* pDataCV = new QChartView(m_pDataC, this);
    pRocLayout->addWidget(pDataCV);
//    pDataCV->setFixedSize(150, 150);

    m_pDataC->setTitle(tr("数据分布"));
    m_pDataC->setTheme(QChart::ChartThemeDark);
    m_pDataC->setAnimationOptions(QChart::NoAnimation);
    m_pDataC->legend()->hide();

    QPieSeries* m_pDataS = new QPieSeries();
    m_pDataC->addSeries(m_pDataS);
    m_pDataS->setPieSize(0.5);
    m_pDataS->setHoleSize(0.25);

    m_tranD = m_pDataS->append(tr("训练集 0"), 1);
    m_badD = m_pDataS->append(tr("剔除数据 0"), 1);
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
    setMinimumSize(900, 400);
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
        if (i%4 != 0)
            datas->trainDatas.append(datas->validDatas[i]);
        else
            datas->testDatas.append(datas->validDatas[i]);

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

    double Max_area = 0.5;
    int bad_index = 0;
    DblVecVec new_trainingdata;
    DblVec new_trainingmeas;
    QString msg;
    for(int time = 0; time < 11; time++)
    {
        msg = QString("\n############\n Begin %1 time training data for find out bad data").arg(time);
        emit datas->logMsg(msg);
        for(int index = 0; index < trainRow; index++)
        {
            // can stop the loop
            while (datas->bStopTraining)
            {
                Sleep(100);
            }

            new_trainingdata.clear();
            new_trainingmeas.clear();
            int trainSize=0;

            if (time == 0)
            {
                msg = QString("  Begin init training");
                emit datas->logMsg(msg);

                new_trainingdata = training_data;
                new_trainingmeas = training_meas;
                trainSize = trainRow;
            }
            else
            {
                msg = QString("  Begin test %1 data for roc").arg(index);
                emit datas->logMsg(msg);

                for(int index_new = 0; index_new < trainRow; index_new++)
                {
                    if(index_new != index)
                    {
                        new_trainingdata.push_back(training_data[index_new]);
                        new_trainingmeas.push_back(training_meas[index_new]);
                        trainSize++;
                    }
                    Sleep(1);
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
//            QSet<double> dataForSortSet;
//            for(int i = 0; i < predictRow; i++)
//                dataForSortSet.insert(predict_meas[i]);
//            QList<double> thresholdList = dataForSortSet.toList();
            QList<double> thresholdList;
            for(int i = 0; i < predictRow; i++)
                thresholdList.append(predict_meas[i]);
            qSort(thresholdList.begin(), thresholdList.end(), qGreater<double>());

            QList<QPointF> rocLine;
            double area = 0, pastTPR = 0, pastFPR = 0;
            for(int j = 0; j < 10; j++)
            {
                double threshold = thresholdList[static_cast<int>(predictRow*0.1*j)];

                int TP = 0, FP = 0, TN = 0, FN = 0;
                int value = 0;
                for(int i = 0; i < predictRow; i++)
                {
                    if (0 == j)
                    {
                        msg = QString("   treal value: %1, predict value: %3")
                                .arg(real_predict_meas[i]).arg(predict_meas[i]);
                        emit datas->logMsg(msg);
                    }

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
                    Sleep(1);
                }
                double FPR = (FP*1.0)/(FP+TN);
                double TPR = (TP*1.0)/(TP+FN);

                rocLine.append(QPointF(FPR, TPR));
                msg = QString("   threshold %4 is: %1, FPR: %2, TPR: %3").arg(threshold).arg(FPR).arg(TPR).arg(j);
                emit datas->logMsg(msg);

                area += (TPR + pastTPR)*(FPR-pastFPR)/2;
                pastTPR = TPR;
                pastFPR = FPR;

                Sleep(1);
            }
            area += (1 + pastTPR)*(1-pastFPR)/2;

            if (0 == time)
            {
                Max_area = area;
            }
            else
            {
                if(area > Max_area)
                {
                    Max_area = area;
                    bad_index = index;

                    datas->bestPredictValues = predict_meas;
                    datas->bestRocLine = datas->numRocLines;

                    msg = QString("Get better roc when delete %1, AUC %2").arg(index).arg(area);
                    emit datas->logMsg(msg);
                }
            }

            msg = QString("  ######## AUC: %1").arg(area);
            emit datas->logMsg(msg);

            datas->currentPredictValues = predict_meas;
            datas->rocLines.append(rocLine);
            datas->rocArea.append(area);
            datas->numRocLines++;

            if (0 == time)
                break;

            Sleep(1);
        }

        Sleep(10);
        datas->iprogress = 340 + 15*time;

        if (0 == time)
            continue;

        //handle for next round.
        if(bad_index > 0)
        {
            msg = tr("\tFind a bad tran data %1, move it").arg(bad_index);
            emit datas->logMsg(msg);
            training_data.erase(training_data.begin()+bad_index);
            training_meas.erase(training_meas.begin()+bad_index);

            datas->badDatas.append(datas->trainDatas[bad_index]);
            datas->trainDatas.remove(bad_index);

            bad_index = 0;
            trainRow--;
        }
        else
        {
            msg = QString("Not found better roc in round: %1").arg(time);
            emit datas->logMsg(msg);
            break;
        }
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
    m_badD->setLabel(tr("剔除数据 0"));

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
        sheetDatas.append(QList<QVariant>());
        for (int j=0; j<m_data.icol; j++)
        {
            sheetDatas[i+1].append(m_data.trainDatas[i][j]);
        }
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
            sheetDatas[i+1].append(m_data.bestPredictValues[i]);
        else
            sheetDatas[i+1].append(m_data.currentPredictValues[i]);
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
    if (m_data.bfinished || m_data.iprogress >= 500)
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
            m_rocL->append(m_data.rocLines[m_data.bestRocLine]);
            m_pRocC->setTitle(tr("Best Roc AUC=%1").arg(m_data.rocArea[m_data.bestRocLine]));
        }
    }

    m_tranD->setValue(m_data.trainDatas.size());
    m_tranD->setLabel(tr("训练集 %1").arg(m_data.trainDatas.size()));
    m_testD->setValue(m_data.testDatas.size());
    m_testD->setLabel(tr("测试集 %1").arg(m_data.testDatas.size()));
    m_repeatD->setValue(m_data.repeatDatas.size());
    m_repeatD->setLabel(tr("重复 %1").arg(m_data.repeatDatas.size()));
    m_errorD->setValue(m_data.errorDatas.size());
    m_errorD->setLabel(tr("残缺 %1").arg(m_data.errorDatas.size()));
    m_badD->setValue(m_data.badDatas.size());
    m_badD->setLabel(tr("剔除数据 %1").arg(m_data.badDatas.size()));

//    int numRoc = m_pRocC->series().size();
//    for ( ; numRoc<m_data.numRocLines; numRoc++)
//    {
//        QLineSeries* rocl = new QLineSeries();
//        m_pRocC->addSeries(rocl);
//        rocl->append(m_data.rocLines[numRoc]);
//    }

    if (m_data.numRocLines > 0 && m_data.currentRocLine != m_data.numRocLines - 1)
    {
        m_data.currentRocLine = m_data.numRocLines - 1;
        m_rocL->clear();
        m_rocL->append(m_data.rocLines[m_data.currentRocLine]);
        m_pRocC->setTitle(tr("Roc AUC=%1").arg(m_data.rocArea[m_data.currentRocLine]));
        m_pRocC->update();
    }

    if (m_pLogUI->document()->lineCount() > 2000)
        m_pLogUI->clear();

    m_pProgressBar->setValue(m_data.iprogress);
}

