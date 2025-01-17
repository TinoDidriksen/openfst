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
// Class to compute the intersection of two FSAs.

#ifndef FST_INTERSECT_H_
#define FST_INTERSECT_H_

#include <algorithm>
#include <vector>

#include <fst/log.h>
#include <fst/arc.h>
#include <fst/cache.h>
#include <fst/compose-filter.h>
#include <fst/compose.h>
#include <fst/connect.h>
#include <fst/float-weight.h>
#include <fst/fst.h>
#include <fst/impl-to-fst.h>
#include <fst/matcher.h>
#include <fst/mutable-fst.h>
#include <fst/properties.h>
#include <fst/state-table.h>
#include <fst/util.h>

namespace fst {

using IntersectOptions = ComposeOptions;

template <class Arc, class M = Matcher<Fst<Arc>>,
          class Filter = SequenceComposeFilter<M>,
          class StateTable =
              GenericComposeStateTable<Arc, typename Filter::FilterState>>
struct IntersectFstOptions
    : public ComposeFstOptions<Arc, M, Filter, StateTable> {
  IntersectFstOptions() = default;

  explicit IntersectFstOptions(const CacheOptions &opts, M *matcher1 = nullptr,
                               M *matcher2 = nullptr, Filter *filter = nullptr,
                               StateTable *state_table = nullptr)
      : ComposeFstOptions<Arc, M, Filter, StateTable>(opts, matcher1, matcher2,
                                                      filter, state_table) {}
};

// Computes the intersection (Hadamard product) of two FSAs. This version is a
// delayed FST. Only strings that are in both automata are retained in the
// result.
//
// The two arguments must be acceptors. One of the arguments must be
// label-sorted.
//
// Complexity: same as ComposeFst.
//
// Caveats: same as ComposeFst.
template <class A>
class IntersectFst : public ComposeFst<A> {
  using Base = ComposeFst<A>;

 public:
  using Arc = A;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Base::CreateBase;
  using Base::CreateBase1;
  using Base::Properties;

  IntersectFst(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
               const CacheOptions &opts = CacheOptions())
      : Base(CreateBase(fst1, fst2, opts)) {
    const bool acceptors =
        fst1.Properties(kAcceptor, true) && fst2.Properties(kAcceptor, true);
    if (!acceptors) {
      FSTERROR() << "IntersectFst: Input FSTs are not acceptors";
      GetMutableImpl()->SetProperties(kError);
    }
  }

  template <class M, class Filter, class StateTable>
  IntersectFst(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
               const IntersectFstOptions<Arc, M, Filter, StateTable> &opts)
      : Base(CreateBase1(fst1, fst2, opts)) {
    const bool acceptors =
        fst1.Properties(kAcceptor, true) && fst2.Properties(kAcceptor, true);
    if (!acceptors) {
      FSTERROR() << "IntersectFst: input FSTs are not acceptors";
      GetMutableImpl()->SetProperties(kError);
    }
  }

  // See Fst<>::Copy() for doc.
  IntersectFst(const IntersectFst &fst, bool safe = false) : Base(fst, safe) {}

  // Get a copy of this IntersectFst. See Fst<>::Copy() for further doc.
  IntersectFst *Copy(bool safe = false) const override {
    return new IntersectFst(*this, safe);
  }

 private:
  using Base::GetImpl;
  using Base::GetMutableImpl;
};

// Specialization for IntersectFst.
template <class Arc>
class StateIterator<IntersectFst<Arc>> : public StateIterator<ComposeFst<Arc>> {
 public:
  explicit StateIterator(const IntersectFst<Arc> &fst)
      : StateIterator<ComposeFst<Arc>>(fst) {}
};

// Specialization for IntersectFst.
template <class Arc>
class ArcIterator<IntersectFst<Arc>> : public ArcIterator<ComposeFst<Arc>> {
 public:
  using StateId = typename Arc::StateId;

  ArcIterator(const IntersectFst<Arc> &fst, StateId s)
      : ArcIterator<ComposeFst<Arc>>(fst, s) {}
};

// Useful alias when using StdArc.
using StdIntersectFst = IntersectFst<StdArc>;

// Computes the intersection (Hadamard product) of two FSAs. This version
// writes the intersection to an output MurableFst. Only strings that are in
// both automata are retained in the result.
//
// The two arguments must be acceptors. One of the arguments must be
// label-sorted.
//
// Complexity: same as Compose.
//
// Caveats: same as Compose.
template <class Arc>
void Intersect(const Fst<Arc> &ifst1, const Fst<Arc> &ifst2,
               MutableFst<Arc> *ofst,
               const IntersectOptions &opts = IntersectOptions()) {
  using M = Matcher<Fst<Arc>>;
  // In each case, we cache only the last state for fastest copy.
  switch (opts.filter_type) {
    case AUTO_FILTER: {
      CacheOptions nopts;
      nopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, nopts);
      break;
    }
    case SEQUENCE_FILTER: {
      IntersectFstOptions<Arc> iopts;
      iopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, iopts);
      break;
    }
    case ALT_SEQUENCE_FILTER: {
      IntersectFstOptions<Arc, M, AltSequenceComposeFilter<M>> iopts;
      iopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, iopts);
      break;
    }
    case MATCH_FILTER: {
      IntersectFstOptions<Arc, M, MatchComposeFilter<M>> iopts;
      iopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, iopts);
      break;
    }
    case NO_MATCH_FILTER: {
      IntersectFstOptions<Arc, M, NoMatchComposeFilter<M>> iopts;
      iopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, iopts);
      break;
    }
    case NULL_FILTER: {
      IntersectFstOptions<Arc, M, NullComposeFilter<M>> iopts;
      iopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, iopts);
      break;
    }
    case TRIVIAL_FILTER: {
      IntersectFstOptions<Arc, M, TrivialComposeFilter<M>> iopts;
      iopts.gc_limit = 0;
      *ofst = IntersectFst<Arc>(ifst1, ifst2, iopts);
      break;
    }
  }
  if (opts.connect) Connect(ofst);
}

}  // namespace fst

#endif  // FST_INTERSECT_H_
