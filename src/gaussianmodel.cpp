#include "gaussianmodel.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <assert.h>
#include <limits>


GaussianModel::GaussianModel(void)
{
    init();
}

void GaussianModel::init()
{
    _iTrainingDataSize = 0;
    _iDimension = 0;
    _dTheta = 0.1;
    _dThetaL = 0.01;
    _dThetaU = 1.0;
    _iKernelType = LINEAR;
    _bOptimizeTheta = true;
    _yTrainMean = 0.0;
    _yTrainStd = 0.0;
}

GaussianModel::~GaussianModel(void)
{
}

bool GaussianModel::train(const DblVecVec& x_train, const DblVec& y_train, const int n, const int p)
{
    _iTrainingDataSize = n;
    _iDimension = p;

    /*Calculate mean and standard deviation of training data set, then normalize the data.*/
    _xTrainMean.resize(_iDimension);
    _xTrainStd.resize(_iDimension);

    DblVec temp_x_train(_iTrainingDataSize * _iDimension);
    DblVec temp_y_train(_iTrainingDataSize);

    for (int i = 0; i < _iTrainingDataSize; ++i)
    {
        for (int j = 0; j < _iDimension; ++j)
        {
            temp_x_train[i * _iDimension + j] = x_train[i][j];
        }
        temp_y_train[i] = y_train[i];
    }
    normalizeAllTrainingData(temp_x_train, temp_y_train);

    /*Compute the difference of the normalized data.*/
    int lower_triangular_size  = _iTrainingDataSize * (_iTrainingDataSize - 1) / 2;
    _xDifferenceNormalized.resize(lower_triangular_size);
    for (int i = 0, row = 1; row < _iTrainingDataSize; ++row)
    {
        for (int col = 0; col < row; ++col, ++i)
        {
            _xDifferenceNormalized[i].resize(_iDimension);
            for (int j = 0; j < _iDimension; ++j)
            {
                _xDifferenceNormalized[i][j] = _xTrainNormalized[row * _iDimension + j] - _xTrainNormalized[col * _iDimension + j];
            }
        }
    }

    if (n > 200) // if all corner number is too large, do not optimize to improve pvt performance
    {
        _bOptimizeTheta = false;
        _dTheta = 0.1;
    }
    if (_bOptimizeTheta)
        optimizeTheta();
    calculateAlpha();
    return true;
}

bool GaussianModel::addTrainingData(const DblVecVec& x_new, const DblVec& y_new, const int n, const int p)
{
    if (p != _iDimension || n < 1)
        return false;

    int new_training_data_size = _iTrainingDataSize + n;
    DblVec temp_x_array(new_training_data_size * _iDimension);
    DblVec temp_y_array(new_training_data_size);

    for (int i = 0; i < _iDimension; ++i)
    {
        for (int j = 0; j < _iTrainingDataSize; ++j)
            temp_x_array[j * _iDimension + i] = (_xTrainNormalized[j * _iDimension + i] * _xTrainStd[i]) + _xTrainMean[i];
        for (int j = _iTrainingDataSize; j < new_training_data_size; ++j)
            temp_x_array[j * _iDimension + i] = x_new[j - _iTrainingDataSize][i];
    }
    for (int i = 0; i < _iTrainingDataSize; ++i)
        temp_y_array[i] = (_yTrainNormalized[i] * _yTrainStd) + _yTrainMean;
    for (int i = _iTrainingDataSize; i < new_training_data_size; ++i)
        temp_y_array[i] = y_new[i - _iTrainingDataSize];
    _iTrainingDataSize = new_training_data_size;
    normalizeAllTrainingData(temp_x_array, temp_y_array);

    int lower_triangular_size  = _iTrainingDataSize * (_iTrainingDataSize - 1) / 2;
    _xDifferenceNormalized.resize(lower_triangular_size);
    for (int i = 0, row = 1; row < _iTrainingDataSize; ++row)
    {
        for (int col = 0; col < row; ++col, ++i)
        {
            _xDifferenceNormalized[i].resize(_iDimension);
            for (int j = 0; j < _iDimension; ++j)
            {
                _xDifferenceNormalized[i][j] = _xTrainNormalized[row * _iDimension + j] - _xTrainNormalized[col * _iDimension + j];
            }
        }
    }
    if (new_training_data_size > 200 ) // if all corner number is too large, do not optimize to improve pvt performance
    {
        _bOptimizeTheta = false;
        _dTheta = 0.1;
    }
    if (_bOptimizeTheta)
        optimizeTheta();
    calculateAlpha();
    return true;
}

void GaussianModel::calculateAlpha(void)
{
    /* Calculate the inverse of the correlation matrix.*/
    int cov_matrix_size = _iTrainingDataSize * _iTrainingDataSize;
    if (static_cast<int>(_xTrainNormalizedCovariance.size()) != cov_matrix_size)
        _xTrainNormalizedCovariance.resize(cov_matrix_size);

    if (_iKernelType == LINEAR)
    {
        // Row
        for (int i = 0; i < _iTrainingDataSize; ++i)
        {
            // Diagonal
            _xTrainNormalizedCovariance[i * _iTrainingDataSize + i] = 1.0;
            // Column
            for (int j = 0; j < i; ++j)
            {
                _xTrainNormalizedCovariance[i * _iTrainingDataSize + j] = corrLinear(_xDifferenceNormalized[(i - 1) * i / 2 + j]);
                _xTrainNormalizedCovariance[j * _iTrainingDataSize + i] = corrLinear(_xDifferenceNormalized[(i - 1) * i / 2 + j]);

            }
        }
    }
    else
    {
        // Row
        for (int i = 0; i < _iTrainingDataSize; ++i)
        {
            // Diagonal
            _xTrainNormalizedCovariance[i * _iTrainingDataSize + i] = 1.0;
            // Column
            for (int j = 0; j < i; ++j)
            {
                _xTrainNormalizedCovariance[i * _iTrainingDataSize + j] = corrSquaredExponential(_xDifferenceNormalized[ ( i - 1 ) * i / 2 + j ]);
                _xTrainNormalizedCovariance[j * _iTrainingDataSize + i] = corrSquaredExponential(_xDifferenceNormalized[ ( i - 1 ) * i / 2 + j ]);
            }
        }
    }

  //  DblVec inv_matrix(cov_matrix_size, 0);
    _lowerTriangle.clear();
    _lowerTriangle.resize(cov_matrix_size);
    _upperTriangle.clear();
    _upperTriangle.resize(cov_matrix_size);
    matrixLUDecomp(_xTrainNormalizedCovariance, _lowerTriangle, _upperTriangle);
   // matrixLUinverse(_xTrainNormalizedCovariance, inv_matrix, _lowerTriangle, _upperTriangle);
#if 0
    for (int i = 0; i < _iTrainingDataSize; ++i)
    {
        _alpha[i] = 0;
        for (int j = 0; j < _iTrainingDataSize; ++j)
            _alpha[i] += inv_matrix[i * _iTrainingDataSize + j] * _yTrainNormalized[j];
    }
#endif
    // Calculate alpha

     DblVec tmpAlpha;
     tmpAlpha.resize(_iTrainingDataSize);
     for(int i = 0; i < _iTrainingDataSize; ++i)
     {
         double sumY = 0;
         for (int j =0 ; j < i; ++j)
             sumY += _lowerTriangle[i*_iTrainingDataSize + j]*tmpAlpha[j];
         tmpAlpha[i] = (_yTrainNormalized[i] - sumY)/_lowerTriangle[i*_iTrainingDataSize + i];
     }
     _alpha.resize(_iTrainingDataSize);
     for (int i = _iTrainingDataSize -1; i >= 0; --i)
     {
         double sumY = 0;
         for(int j = _iTrainingDataSize -1; j > i ; --j)
             sumY += _upperTriangle[i*_iTrainingDataSize + j]*_alpha[j];
         _alpha[i] = (tmpAlpha[i] - sumY)/_upperTriangle[i*_iTrainingDataSize + i];
     }
}

bool GaussianModel::predict( const DblVecVec &x_test, const int n, DblVec &y_test )
{
    if ( _iDimension == 0 || n < 1 )
        return false;

    DblVec x_test_normalized( n * _iDimension );
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < _iDimension; ++j)
            x_test_normalized[i * _iDimension + j] = (x_test[i][j] - _xTrainMean[j]) / _xTrainStd[j];
    }

    if (_iKernelType == LINEAR)
    {
        for ( int i = 0; i < n; ++i )
        {
            y_test[i] = 0.0;
            DblVec temp_x_difference(_iDimension);
            for (int j = 0; j < _iTrainingDataSize; ++j)
            {
                for (int k = 0; k < _iDimension; ++k)
                    temp_x_difference[k] = x_test_normalized[i * _iDimension + k] - _xTrainNormalized[j * _iDimension + k];
                y_test[i] += _alpha[j] * corrLinear(temp_x_difference);
            }
            y_test[i] = y_test[i] * _yTrainStd + _yTrainMean;
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            y_test[i] = 0.0;
            DblVec temp_x_difference(_iDimension);
            for (int j = 0; j < _iTrainingDataSize; ++j)
            {
                for (int k = 0; k < _iDimension; ++k)
                    temp_x_difference[k] = x_test_normalized[i * _iDimension + k] - _xTrainNormalized[j * _iDimension + k];
                y_test[i] += _alpha[j] * corrSquaredExponential(temp_x_difference);
            }
            y_test[i] = y_test[i] * _yTrainStd + _yTrainMean;
        }
    }

    return true;
}

void GaussianModel::setKernelType(int kernel_type)
{
    _iKernelType = kernel_type;

    // Re-calculate alpha
    calculateAlpha();
}

void GaussianModel::setOptimizeTheta(bool optimize_theta)
{
    _bOptimizeTheta = optimize_theta;
}

void GaussianModel::setTheta(double theta)
{
    _dTheta = theta;
}

void GaussianModel::setThetaL(double theta_l)
{
    _dThetaL = theta_l;
}

void GaussianModel::setThetaU(double theta_u)
{
    _dThetaU = theta_u;
}

double GaussianModel::mean(const DblVec &data)
{
    int n = static_cast<int>(data.size());
    if (n < 1)
        return 0.0;

    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        sum += data[i];
    }

    return sum / static_cast<double>(n);
}

double GaussianModel::std(const DblVec &data)
{
    int n = static_cast<int>(data.size());
    if ( n < 2 )
        return 0.0;

    double Mean = mean(data);
    double sum_square = 0.0;
    for ( int i = 0; i < n; ++i )
        sum_square += (data[i] - Mean) * (data[i] - Mean);

    // take the training data set as a population, use biased estimator
    // return sqrt( sum_square / static_cast<double>( n - 1 ) );
    return sqrt(sum_square / static_cast<double>(n));
}

bool GaussianModel::normalizeAllTrainingData(const DblVec &x_train, const DblVec &y_train)
{
    if ( y_train.size() == 0 || static_cast<int>(y_train.size() ) != _iTrainingDataSize
         || static_cast<int>(x_train.size() / y_train.size()) != _iDimension)
        return false;

    _xTrainNormalized.resize(_iTrainingDataSize * _iDimension);
    _yTrainNormalized.resize(_iTrainingDataSize);
    DblVec temp_array(_iTrainingDataSize);
    for (int i = 0; i < _iDimension; ++i)
    {
        for (int j = 0; j < _iTrainingDataSize; ++j)
            temp_array[j] = x_train[j * _iDimension + i];
        _xTrainMean[i] = mean(temp_array);
        _xTrainStd[i] = std(temp_array);
        assert(fabs(_xTrainStd[i]) > 0);
    }
    for (int i = 0; i < _iTrainingDataSize; ++i)
        temp_array[i] = y_train[i];
    _yTrainMean = mean(temp_array);
    _yTrainStd = std(temp_array);
    assert(fabs(_yTrainStd) > 0);

    // Normalize data
    for (int i = 0; i < _iTrainingDataSize; ++i)
    {
        for (int j = 0; j < _iDimension; ++j)
        {
            if(fabs(_xTrainStd[j]) > 0)
                _xTrainNormalized[i * _iDimension + j] = (x_train[i * _iDimension + j] - _xTrainMean[j]) / _xTrainStd[j];
            else
                _xTrainNormalized[i * _iDimension + j] = 1;
        }
    }
    for (int i = 0; i < _iTrainingDataSize; ++i)
    {
        if(fabs(_yTrainStd) > 0)
            _yTrainNormalized[i] = (y_train[i] - _yTrainMean) / _yTrainStd;
        else
            _yTrainNormalized[i] = 1;
    }

    return true;
}

double GaussianModel::corrLinear(const DblVec &d)
{
    double corr = 1.0;
    for (int i = 0; i < _iDimension; ++i)
    {

        double td_temp = _dTheta * fabs(d[i]);                   //yeyun use "abs". ????????
        if (td_temp > 1.0)
        {
            corr = 0.0;
            break;
        }
        else
            corr *= 1 - td_temp;
    }
    return corr;
}

double GaussianModel::corrSquaredExponential(const DblVec &d)
{
    double sum_square = 0.0;
    for (int i = 0; i < _iDimension; ++i)
        sum_square += d[i] * d[i];
    return exp( -_dTheta * sum_square );
}

bool GaussianModel::matrixLUinverse(const DblVec &matrix, DblVec &inv_matrix, DblVec &lowerTriangle, DblVec &upperTriangle)
{
    inv_matrix.clear();

    if(_iTrainingDataSize < 1)
    {
        return false;
    }
    else if (_iTrainingDataSize == 1)
    {
        if(fabs(matrix[0]) > 0)
            inv_matrix[0] = 1/matrix[0];
        else
            return false;
    }
    else
    {
        int i, j, a, b;
        DblVec &L = lowerTriangle;
        DblVec &U = upperTriangle;
        DblVec d(_iTrainingDataSize), x(_iTrainingDataSize), e(_iTrainingDataSize);
        double tmp;
        if(matrixLUDecomp(matrix, L, U))
        {
            for(i = 0; i < _iTrainingDataSize; i++)
                x[i] = d[i] = 0;
            for(i = 0; i < _iTrainingDataSize; i++)
            {
                for(j = 0; j < _iTrainingDataSize; j++)
                    e[j] = 0;
                e[i] = 1;
                j = 0;
                b = 0;
                while(j < _iTrainingDataSize)
                {
                    tmp = 0;
                    a = 0;
                    while(a < j)
                    {
                        tmp += d[a]*L[b+a];
                        a++;
                    }
                    d[j] = e[j] - tmp;
                    d[j] /= L[b+j];
                    j++;
                    b += _iTrainingDataSize;
                }
                j = _iTrainingDataSize - 1;
                b -= _iTrainingDataSize;
                while(j > -1)
                {
                    tmp = 0;
                    a = j + 1;
                    while(a < _iTrainingDataSize)
                    {
                        tmp += U[b+a]*x[a];
                        a++;
                    }
                    x[j] = d[j] - tmp;
                    x[j] /= U[b+j];
                    j--;
                    b -= _iTrainingDataSize;
                }
                for(j=0, b=i; j < _iTrainingDataSize; j++)
                {
                    inv_matrix[b] = x[j];
                    b += _iTrainingDataSize;
                }
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

double GaussianModel::covMatrixDet()
{
   // int cov_matrix_size = _iTrainingDataSize * _iTrainingDataSize;
   // DblVec lower_triangle(cov_matrix_size, 0);
   // DblVec upper_triangle(cov_matrix_size, 0);
   //matrixLUDecomp(matrix, lower_triangle, upper_triangle);
    double det = 1.0;
    for (int i = 0; i < _iTrainingDataSize; ++i)
    {
        if(fabs(_lowerTriangle[i * _iTrainingDataSize + i]) > 0)
            det *= _lowerTriangle[i * _iTrainingDataSize + i];
    // (-1)^s is not calculated here because covariance matrix will not return negative determinant
    }
    return fabs(det);
}

bool GaussianModel::matrixLUDecomp(const DblVec &matrix, DblVec &lower_triangle, DblVec &upper_triangle)
{
    //DblVec upper_triangle;
    //upper_triangle.resize(_iTrainingDataSize*_iTrainingDataSize);

    int i, j, a, b, c, d;
    double tmp;
    for(i = 0, a = 0; i < _iTrainingDataSize; i++)
    {
        for(j = 0; j < _iTrainingDataSize; j++)
            lower_triangle[a+j] = upper_triangle[a+j] = 0;
        upper_triangle[a+i] = 1;
        a += _iTrainingDataSize;
    }
    for(j = 0, d = 0; j < _iTrainingDataSize; j++)
    {
        for(i = j, b = d; i < _iTrainingDataSize; i++)
        {
            tmp = 0;
            a = 0;
            c = j;
            while(a < j)
            {
                tmp += lower_triangle[b+a]*upper_triangle[c];
                c += _iTrainingDataSize;
                a++;
            }
            lower_triangle[b+j] = matrix[b+j] - tmp;
            b += _iTrainingDataSize;
        }
        i = j + 1;
        while(i < _iTrainingDataSize)
        {
            tmp = 0;
            a = 0;
            c = i;
            while( a < j)
            {
                tmp += lower_triangle[d+a]*upper_triangle[c];
                a++;
                c+=_iTrainingDataSize;
            }
            if(fabs(lower_triangle[d+j]) > 0)
                upper_triangle[d+i]=(matrix[d+i]-tmp)/lower_triangle[d+j];
            else
                return false;                               // can not be decomposition.
            i++;
        }
        d += _iTrainingDataSize;
    }
    return true;
}

void GaussianModel::optimizeTheta(void)
{
    // overall searching for the maxmium
    double theta0 = _dTheta;
    double lf0 = likelihoodFunction();
    double d_theta = (_dThetaU - _dThetaL) / 5.0;
    double lf = 0.0;

    for (_dTheta = _dThetaL; _dTheta < _dThetaU; _dTheta += d_theta)
    {
        lf = likelihoodFunction();
        if (lf0 < lf)
        {
            lf0 = lf;
            theta0 = _dTheta;
        }
    }
    _dTheta = theta0;

    double dt = 0.0001;
    _dTheta -= dt / 2.0;
    double lf_minus = likelihoodFunction();
    _dTheta += dt;
    double lf_plus = likelihoodFunction();
    _dTheta -= dt / 2.0;
    double d_rate = (lf_plus - lf_minus) / dt;
    lf = (lf_minus + lf_plus) / 2.0;

    double minimum_t_step = 1.0e-4;
    double t_step = _dTheta / 20.0;

    int optimizeIndex = 100;
    while (t_step > minimum_t_step && optimizeIndex > 0)
    {
        double last_theta = _dTheta;
        double last_lf = lf;
        if (d_rate > 0.0)
            _dTheta += t_step;
        else
            _dTheta -= t_step;

        _dTheta -= dt / 2.0;
        lf_minus = likelihoodFunction();
        _dTheta += dt;
        lf_plus = likelihoodFunction();
        _dTheta -= dt / 2.0;
        d_rate = (lf_plus - lf_minus) / dt;
        lf = (lf_minus + lf_plus) / 2.0;

        if (lf < last_lf || _dTheta < _dThetaL || _dTheta > _dThetaU)
        {
            _dTheta = last_theta;
            t_step /= 10.0;
        }
        else
        {
            optimizeIndex--;
        }
    }
}

double GaussianModel::likelihoodFunction(void)
{
    // The basic marginal likelihood function
    // For better prediction of circuit performance, Vincent Dubourg's reduced likelihood function may be the best candidate
    // will try it later
    return marginalLikelihoodFunction();
}

double GaussianModel::marginalLikelihoodFunction(void)
{
    calculateAlpha();
    double yky = 0.0;
    for (int i = 0; i < _iTrainingDataSize; ++i)
        yky += _yTrainNormalized[i] * _alpha[i];
    double log_det_k = log( covMatrixDet());
    return -0.5 * ( yky + log_det_k ) / static_cast<double>(_iTrainingDataSize);
}


QDataStream &operator<<(QDataStream &pout, const GaussianModel &item)
{
    pout << item._iTrainingDataSize << item._iDimension << item._dTheta << item._dThetaL << item._dThetaU
         << item._iKernelType << item._bOptimizeTheta << item._xTrainMean << item._xTrainStd << item._yTrainMean
         << item._yTrainStd << item._alpha << item._xTrainNormalized << item._yTrainNormalized
         << item._xDifferenceNormalized << item._xTrainNormalizedCovariance<< item._lowerTriangle
         << item._upperTriangle;
    return pout;
}


QDataStream &operator>>(QDataStream &pin, GaussianModel &item)
{
    pin >> item._iTrainingDataSize >> item._iDimension >> item._dTheta >> item._dThetaL >> item._dThetaU
         >> item._iKernelType >> item._bOptimizeTheta >> item._xTrainMean >> item._xTrainStd >> item._yTrainMean
         >> item._yTrainStd >> item._alpha >> item._xTrainNormalized >> item._yTrainNormalized
         >> item._xDifferenceNormalized >> item._xTrainNormalizedCovariance>> item._lowerTriangle
         >> item._upperTriangle;
    return pin;
}
