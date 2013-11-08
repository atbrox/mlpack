/**
 * @file logistic_regression_impl.hpp
 * @author Sumedh Ghaisas
 *
 * Implementation of the LogisticRegression class.
 */
#ifndef __MLPACK_METHODS_LOGISTIC_REGRESSION_LOGISTIC_REGRESSION_IMPL_HPP
#define __MLPACK_METHODS_LOGISTIC_REGRESSION_LOGISTIC_REGRESSION_IMPL_HPP

// In case it hasn't been included yet.
#include "logistic_regression.hpp"

namespace mlpack {
namespace regression {

template<template<typename> class OptimizerType>
LogisticRegression<OptimizerType>::LogisticRegression(
    arma::mat& predictors,
    arma::vec& responses,
    const double lambda) :
    predictors(predictors),
    responses(responses),
    errorFunction(LogisticRegressionFunction(predictors,responses,lambda)),
    optimizer(OptimizerType<LogisticRegressionFunction>(errorFunction)),
    lambda(lambda)
{
  parameters.zeros(predictors.n_rows + 1);
}

template<template<typename> class OptimizerType>
LogisticRegression<OptimizerType>::LogisticRegression(
    arma::mat& predictors,
    arma::vec& responses,
    const arma::mat& initialPoint,
    const double lambda) :
    predictors(predictors),
    responses(responses),
    errorFunction(LogisticRegressionFunction(predictors,responses)),
    optimizer(OptimizerType<LogisticRegressionFunction>(errorFunction)),
    lambda(lambda)
{
  parameters.zeros(predictors.n_rows + 1);
}

template <template<typename> class OptimizerType>
double LogisticRegression<OptimizerType>::LearnModel()
{
  //add rows of ones to predictors
  arma::rowvec ones;
  ones.ones(predictors.n_cols);
  predictors.insert_rows(0, ones);

  double out;
  Timer::Start("logistic_regression_optimization");
  out = optimizer.Optimize(parameters);
  Timer::Stop("logistic_regression_optimization");

  //shed the added rows
  predictors.shed_row(0);

  return out;
}

template <template<typename> class OptimizerType>
double LogisticRegression<OptimizerType>::ComputeError(
    arma::mat& predictors,
    const arma::vec& responses)
{
  // Here we add the row of ones to the predictors.
  arma::rowvec ones;
  ones.ones(predictors.n_cols);
  predictors.insert_rows(0, ones);

  double out = errorFunction.Evaluate(predictors,responses,parameters);

  predictors.shed_row(0);

  return out;
}

template <template<typename> class OptimizerType>
double LogisticRegression<OptimizerType>::ComputeAccuracy(
    arma::mat& predictors,
    const arma::vec& responses,
    const double decisionBoundary)
{
  arma::vec temp_responses;
  Predict(predictors,temp_responses,decisionBoundary);
  int count = 0;
  for (size_t i = 0; i < responses.n_rows; i++)
    if (responses(i, 0) == temp_responses(i, 0))
      count++;

  return (double) (count * 100) / responses.n_rows;
}

template <template<typename> class OptimizerType>
void LogisticRegression<OptimizerType>::Predict(
    arma::mat& predictors,
    arma::vec& responses,
    const double decisionBoundary)
{
  //add rows of ones to predictors
  arma::rowvec ones;
  ones.ones(predictors.n_cols);
  predictors.insert_rows(0, ones);

  responses = arma::floor(
      errorFunction.getSigmoid(arma::trans(predictors) * parameters) -
      decisionBoundary * arma::ones<arma::vec>(predictors.n_cols, 1)) +
      arma::ones<arma::vec>(predictors.n_cols);

  //shed the added rows
  predictors.shed_row(0);
}

}; // namespace regression
}; // namespace mlpack

#endif // __MLPACK_METHODS_LOGISTIC_REGRESSION_LOGISTIC_REGRESSION_IMPL_HPP
