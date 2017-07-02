#ifndef GAUSSIANMODEL_H
#define GAUSSIANMODEL_H

#include "QVector"
#include <cmath>
#include "QDataStream"
using std::vector;
using std::abs;
using std::sqrt;
using std::exp;
using std::log;

class GaussianModel
{
public:
    typedef QVector<double>     DblVec;
    typedef QVector<DblVec>     DblVecVec;

    // Enum type for kernel function
    enum CorrelationType
    {
        LINEAR = 0,
        SQUARED_EXPONENTIAL = 1
    };

    GaussianModel( void );
    void init();
    virtual ~GaussianModel();
    // train the model with input training data set. The old training data set will be removed
    bool train( const DblVecVec& x_train, const DblVec& y_train, const int n, const int p );
    // Add data to training data set. It is the client's responsibility to ensure the correct data format
    bool addTrainingData( const DblVecVec& x_new, const DblVec& y_new, const int n, const int p );
    bool predict( const DblVecVec &x_test, const int n, DblVec &y_test );
    // It is the client's responsibility to update according data after the kernel type changed
    void setKernelType( int kernel_type );
    void setOptimizeTheta( bool optimize_theta );
    void setTheta( double theta );
    void setThetaL( double theta_l );
    void setThetaU( double theta_u );
    double std( const DblVec &data );

    friend QDataStream &operator<<(QDataStream &pout, const GaussianModel &item);
    friend QDataStream &operator>>(QDataStream &pin, GaussianModel &item);

private:
    double mean( const DblVec &data );
    bool normalizeAllTrainingData( const DblVec &x_train, const DblVec &y_train );
    void calculateAlpha( void );
    double corrLinear( const DblVec &d );
    double corrSquaredExponential( const DblVec &d );
    bool matrixLUinverse(const DblVec &matrix, DblVec &inv_matrix, DblVec &lower_triangle, DblVec &upper_triangle);
    double covMatrixDet();
    bool matrixLUDecomp( const DblVec &matrix, DblVec &lower_triangle, DblVec &upper_triangle);
    void optimizeTheta( void );
    double likelihoodFunction( void );
    double marginalLikelihoodFunction( void );

    // Coefficient in kernel function, the empirical value is ~0.1
    int _iTrainingDataSize;
    int _iDimension;

    // The correlation function for prediction
    double _dTheta;
    double _dThetaL;
    double _dThetaU;
    int _iKernelType;
    bool _bOptimizeTheta;

    DblVec _xTrainMean;
    DblVec _xTrainStd;
    double _yTrainMean;
    double _yTrainStd;
    // prediction coefficients
    DblVec _alpha;
    // Normalized x_train data. The data is stored like the following:
    // d0 d1 d2 d0 d1 d2 d0 d1 d2 ...
    //| data0  | data1  | data2  | ...
    DblVec _xTrainNormalized;
    DblVec _yTrainNormalized;
    // The difference of training data X for norm calculation
    // The full difference data is a matrix, 1-d vector<double> array here for efficiency
    // The size of this array is n * ( n + 1 ) / 2
    // A 4*4 example, size = 4 * 3 / 2 = 6:
    //
    // I x x x
    // 0 I x x
    // 1 2 I x
    // 3 4 5 I
    //
    // Not needed if hyper parameter optimization is not adopted
    DblVecVec _xDifferenceNormalized;
    DblVec _xTrainNormalizedCovariance;
    DblVec _lowerTriangle;
    DblVec _upperTriangle;
};

#endif // GAUSSIANMODEL_H
