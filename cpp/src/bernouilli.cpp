#include <gsl/gsl_sf_gamma.h>
#include "bernouilli.h"

LogProb logBetaBernouilli (double alpha, double beta, double succ, double fail) {
  return gsl_sf_lnbeta (alpha + succ, beta + fail) - gsl_sf_lnbeta (alpha, beta);
}

set<BernouilliParamIndex> BernouilliCounts::combinedIndices (const BernouilliCounts& other) const {
  set<BernouilliParamIndex> idx = allIndices(), otherIdx = other.allIndices();
  idx.insert (otherIdx.begin(), otherIdx.end());
  return idx;
}

set<BernouilliParamIndex> BernouilliCounts::allIndices() const {
  set<BernouilliParamIndex> idx;
  for (auto& pc : succ)
    idx.insert (pc.first);
  for (auto& pc : fail)
    idx.insert (pc.first);
  return idx;
}

LogProb BernouilliCounts::logBetaBernouilli (const BernouilliCounts& prior) const {
  auto priorSucc(prior.succ), priorFail(prior.fail),
    mySucc(succ), myFail(fail);  // make copies to leverage default-constructible properties of map
  LogProb lp = 0;
  for (auto n : combinedIndices(prior))
    lp += ::logBetaBernouilli (priorSucc[n] + 1, priorFail[n] + 1, mySucc[n], myFail[n]);
  return lp;
}

LogProb BernouilliCounts::deltaLogBetaBernouilli (const BernouilliCounts& delta) const {
  auto deltaSucc(delta.succ), deltaFail(delta.fail),
    oldSucc(succ), oldFail(fail);  // make copies to leverage default-constructible properties of map
  LogProb lp = 0;
  for (auto n : combinedIndices(delta))
    lp += gsl_sf_lnbeta (oldSucc[n] + deltaSucc[n] + 1, oldFail[n] + deltaFail[n] + 1)
      - gsl_sf_lnbeta (oldSucc[n] + 1, oldFail[n] + 1);
  return lp;
}

BernouilliCounts& BernouilliCounts::operator+= (const BernouilliCounts& c) {
  for (auto pc : c.succ)
    if ((succ[pc.first] += pc.second) == 0)
      succ.erase (pc.first);
  for (auto pc : c.fail)
    if ((fail[pc.first] += pc.second) == 0)
      fail.erase (pc.first);
  return *this;
}
