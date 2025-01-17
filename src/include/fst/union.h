// Copyright 2005-2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Functions and classes to compute the union of two FSTs.

#ifndef FST_UNION_H_
#define FST_UNION_H_

#include <algorithm>
#include <utility>
#include <vector>

#include <fst/log.h>
#include <fst/arc.h>
#include <fst/cache.h>
#include <fst/expanded-fst.h>
#include <fst/float-weight.h>
#include <fst/fst.h>
#include <fst/impl-to-fst.h>
#include <fst/mutable-fst.h>
#include <fst/properties.h>
#include <fst/rational.h>
#include <fst/symbol-table.h>
#include <fst/util.h>

namespace fst {

// Computes the union (sum) of two FSTs. This version writes the union to an
// output MutableFst. If A transduces string x to y with weight a and B
// transduces string w to v with weight b, then their union transduces x to y
// with weight a and w to v with weight b.
//
// Complexity:
//
//   Time: (V_2 + E_2)
//   Space: O(V_2 + E_2)
//
// where Vi is the number of states, and Ei is the number of arcs, in the ith
// FST.
template <class Arc>
void Union(MutableFst<Arc> *fst1, const Fst<Arc> &fst2) {
  // Checks for symbol table compatibility.
  if (!CompatSymbols(fst1->InputSymbols(), fst2.InputSymbols()) ||
      !CompatSymbols(fst1->OutputSymbols(), fst2.OutputSymbols())) {
    FSTERROR() << "Union: Input/output symbol tables of 1st argument "
               << "do not match input/output symbol tables of 2nd argument";
    fst1->SetProperties(kError, kError);
    return;
  }
  const auto numstates1 = fst1->NumStates();
  const bool initial_acyclic1 =
      fst1->Properties(kInitialAcyclic, false) == kInitialAcyclic;
  const auto props1 = fst1->Properties(kFstProperties, false);
  const auto props2 = fst2.Properties(kFstProperties, false);
  const auto start2 = fst2.Start();
  if (start2 == kNoStateId) {
    if (props2 & kError) fst1->SetProperties(kError, kError);
    return;
  }
  if (std::optional<typename Arc::StateId> numstates2 =
          fst2.NumStatesIfKnown()) {
    fst1->ReserveStates(numstates1 + *numstates2 + (initial_acyclic1 ? 0 : 1));
  }
  for (StateIterator<Fst<Arc>> siter(fst2); !siter.Done(); siter.Next()) {
    const auto s1 = fst1->AddState();
    const auto s2 = siter.Value();
    fst1->SetFinal(s1, fst2.Final(s2));
    fst1->ReserveArcs(s1, fst2.NumArcs(s2));
    for (ArcIterator<Fst<Arc>> aiter(fst2, s2); !aiter.Done(); aiter.Next()) {
      auto arc = aiter.Value();  // Copy intended.
      arc.nextstate += numstates1;
      fst1->AddArc(s1, std::move(arc));
    }
  }
  const auto start1 = fst1->Start();
  if (start1 == kNoStateId) {
    fst1->SetStart(start2);
    fst1->SetProperties(props2, kCopyProperties);
    return;
  }
  if (initial_acyclic1) {
    fst1->AddArc(start1, Arc(0, 0, start2 + numstates1));
  } else {
    const auto nstart1 = fst1->AddState();
    fst1->SetStart(nstart1);
    fst1->AddArc(nstart1, Arc(0, 0, start1));
    fst1->AddArc(nstart1, Arc(0, 0, start2 + numstates1));
  }
  fst1->SetProperties(UnionProperties(props1, props2), kFstProperties);
}

// Same as the above but can handle arbitrarily many right-hand-side FSTs,
// preallocating the states.
template <class Arc>
void Union(MutableFst<Arc> *fst1, const std::vector<const Fst<Arc> *> &fsts2) {
  // We add 1 just in case fst1 has an initial cycle.
  fst1->ReserveStates(1 + fst1->NumStates() + CountStates(fsts2));
  for (const auto *fst2 : fsts2) Union(fst1, *fst2);
}

// Computes the union of two FSTs, modifying the RationalFst argument.
template <class Arc>
void Union(RationalFst<Arc> *fst1, const Fst<Arc> &fst2) {
  fst1->GetMutableImpl()->AddUnion(fst2);
}

using UnionFstOptions = RationalFstOptions;

// Computes the union (sum) of two FSTs. This version is a delayed FST. If A
// transduces string x to y with weight a and B transduces string w to v with
// weight b, then their union transduces x to y with weight a and w to v with
// weight b.
//
// Complexity:
//
//   Time: O(v_1 + e_1 + v_2 + e_2)
//   Space: O(v_1 + v_2)
//
// where vi is the number of states visited, and ei is the number of arcs
// visited, in the ith FST. Constant time and space to visit an input state or
// arc is assumed and exclusive of caching.
template <class A>
class UnionFst : public RationalFst<A> {
  using Base = RationalFst<A>;

 public:
  using Arc = A;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  UnionFst(const Fst<Arc> &fst1, const Fst<Arc> &fst2) {
    GetMutableImpl()->InitUnion(fst1, fst2);
  }

  UnionFst(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
           const UnionFstOptions &opts)
      : Base(opts) {
    GetMutableImpl()->InitUnion(fst1, fst2);
  }

  // See Fst<>::Copy() for doc.
  UnionFst(const UnionFst &fst, bool safe = false) : Base(fst, safe) {}

  // Gets a copy of this UnionFst. See Fst<>::Copy() for further doc.
  UnionFst *Copy(bool safe = false) const override {
    return new UnionFst(*this, safe);
  }

 private:
  using Base::GetImpl;
  using Base::GetMutableImpl;
};

// Specialization for UnionFst.
template <class Arc>
class StateIterator<UnionFst<Arc>> : public StateIterator<RationalFst<Arc>> {
 public:
  explicit StateIterator(const UnionFst<Arc> &fst)
      : StateIterator<RationalFst<Arc>>(fst) {}
};

// Specialization for UnionFst.
template <class Arc>
class ArcIterator<UnionFst<Arc>> : public ArcIterator<RationalFst<Arc>> {
 public:
  using StateId = typename Arc::StateId;

  ArcIterator(const UnionFst<Arc> &fst, StateId s)
      : ArcIterator<RationalFst<Arc>>(fst, s) {}
};

using StdUnionFst = UnionFst<StdArc>;

}  // namespace fst

#endif  // FST_UNION_H_
