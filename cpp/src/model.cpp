#include "model.h"

Parameterization::Parameterization (const Assocs& assocs)
  : termPrior (assocs.terms(), 0),
    geneFalsePos (assocs.genes(), 1),
    geneFalseNeg (assocs.genes(), 2)
{
  params.addParam ("t");
  params.addParam ("fp");
  params.addParam ("fn");
}

Model::Model (const Assocs& assocs, const Parameterization& param)
  : assocs (assocs),
    parameterization (param),
    termName (assocs.ontology.termName),
    geneName (assocs.geneName),
    inGeneSet (assocs.genes(), false),
    isRelevant (assocs.terms(), false),
    relevantNeighbors (assocs.terms())
{ }

void Model::init (const GeneNameSet& geneNames) {
  set<GeneName> missing;
  for (auto& n : geneNames)
    if (assocs.geneIndex.count(n))
      geneSet.insert (assocs.geneIndex.at(n));
    else
      missing.insert (n);
  if (missing.size())
    throw runtime_error ((string("Genes not found in the associations list: ") + join(missing)).c_str());

  set<TermIndex> relevant;
  for (auto g : geneSet) {
    inGeneSet[g] = true;
    _falseGenes.insert (g);
    for (auto t : assocs.termsByGene[g])
      relevant.insert (t);
  }

  relevantTerms = vguard<TermIndex> (relevant.begin(), relevant.end());
  for (auto t : relevantTerms)
    isRelevant[t] = true;
  for (auto t : relevantTerms) {
    for (auto p : assocs.ontology.parents[t])
      if (isRelevant[p])
	relevantNeighbors[t].push_back (p);
    for (auto c : assocs.ontology.children[t])
      if (isRelevant[c])
	relevantNeighbors[t].push_back (c);
  }
}

void Model::setTermState (TermIndex t, bool val) {
  Assert (isRelevant[t], "Attempt to set non-relevant term %s", assocs.ontology.termName[t].c_str());
  if (termState[t] != val) {
    const int delta = val ? +1 : -1;
    for (auto g : assocs.genesByTerm[t]) {
      const int newCount = (nActiveTermsByGene[g] += delta);
      const bool gInSet = inGeneSet[g],
	gFalse = (newCount > 0 ? !gInSet : gInSet);
      if (gFalse)
	_falseGenes.insert (g);
      else
	_falseGenes.erase (g);
    }
    if (val)
      _activeTerms.insert (t);
    else
      _activeTerms.erase (t);
    termState[t] = val;
  }
}

void Model::setTermStates (const TermStateAssignment& tsa) {
  for (auto& ts : tsa)
    setTermState (ts.first, ts.second);
}

Model::TermStateAssignment Model::invert (const TermStateAssignment& tsa) const {
  Model::TermStateAssignment inv;
  for (auto& ts : tsa)
    if (ts.second != termState[ts.first])
      inv[ts.first] = termState[ts.first];
  return inv;
}

void Model::countTerm (BernoulliCounts& counts, int inc, TermIndex t, bool state) const {
  auto& countMap = state ? counts.succ : counts.fail;
  BernoulliParamIndex countParam = parameterization.termPrior[t];
  if ((countMap[countParam] += inc) == 0)
    countMap.erase (countParam);
}

void Model::countObs (BernoulliCounts& counts, int inc, bool isActive, GeneIndex g) const {
  const bool gInSet = inGeneSet[g],
    isFalse = isActive ? !gInSet : gInSet;
  auto& countMap = isFalse ? counts.succ : counts.fail;
  BernoulliParamIndex countParam = (isActive ? parameterization.geneFalseNeg : parameterization.geneFalsePos)[g];
  if ((countMap[countParam] += inc) == 0)
    countMap.erase (countParam);
}
    
BernoulliCounts Model::getCounts() const {
  BernoulliCounts counts;
  for (auto t : relevantTerms)
    countTerm (counts, +1, t, termState[t]);
  for (GeneIndex g = 0; g < genes(); ++g)
    countObs (counts, +1, nActiveTermsByGene[g] > 0, g);
  return counts;
}

BernoulliCounts Model::getCountDelta (const TermStateAssignment& tsa) const {
  map<GeneIndex,int> newActiveTermsByGene;
  BernoulliCounts cd;
  for (auto& ts : tsa) {
    const TermIndex t = ts.first;
    const bool val = ts.second;
    Assert (isRelevant[t], "Attempt to set non-relevant term %s", assocs.ontology.termName[t].c_str());
    if (termState[t] != val) {
      countTerm (cd, -1, t, termState[t]);
      countTerm (cd, +1, t, val);
      const int delta = val ? +1 : -1;
      for (auto g : assocs.genesByTerm[t]) {
	const int oldCount = newActiveTermsByGene.count(g) ? newActiveTermsByGene[g] : nActiveTermsByGene[g];
	const int newCount = (newActiveTermsByGene[g] += delta);
	const bool oldActive = oldCount > 0, newActive = newCount > 0;
	if (oldActive != newActive) {
	  countObs (cd, -1, oldActive, g);
	  countObs (cd, +1, newActive, g);
	}
      }
    }
  }
  return cd;
}

void Model::proposeFlipMove (Move& move, RandomGenerator& generator) const {
  TermIndex term = random_element (relevantTerms, generator);
  move.termStates[term] = !termState[term];
}

void Model::proposeSwapMove (Move& move, RandomGenerator& generator) const {
  if (!_activeTerms.empty()) {
    const vguard<TermIndex> actives (_activeTerms.begin(), _activeTerms.end());
    const TermIndex term = random_element (actives, generator);
    const vguard<TermIndex>& nbrs = relevantNeighbors[term];
    if (!nbrs.empty()) {
      const TermIndex nbr = random_element (nbrs, generator);
      if (!termState[nbr]) {
	move.termStates[term] = false;
	move.termStates[nbr] = true;
	move.proposalHastingsRatio = nbrs.size() / (double) relevantNeighbors[nbr].size();
      }
    }
  }
}

void Model::proposeRandomizeMove (Move& move, RandomGenerator& generator) const {
  bernoulli_distribution distrib (0.5);
  for (auto t : relevantTerms)
    move.termStates[t] = distrib(generator);
}

bool Model::sampleMoveCollapsed (Move& move, BernoulliCounts& counts, RandomGenerator& generator) {
  move.delta = getCountDelta (move.termStates);
  move.logLikelihoodRatio = counts.deltaLogBetaBernoulli (move.delta);
  move.hastingsRatio = move.proposalHastingsRatio * exp (move.logLikelihoodRatio);
  if (move.hastingsRatio >= 1 || bernoulli_distribution(move.hastingsRatio)(generator)) {
    setTermStates (move.termStates);
    move.accepted = true;
    counts += move.delta;
  } else
    move.accepted = false;
  return move.accepted;
}