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
// Generic FST augmented with state count-interface class definition.

#ifndef FST_EXPANDED_FST_H_
#define FST_EXPANDED_FST_H_

#include <sys/types.h>

#include <cstddef>
#include <ios>
#include <iostream>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fst/log.h>
#include <fst/arc.h>
#include <fstream>
#include <fst/fst.h>
#include <fst/impl-to-fst.h>
#include <fst/properties.h>
#include <fst/register.h>

namespace fst {

// A generic FST plus state count.
template <class A>
class ExpandedFst : public Fst<A> {
 public:
  using Arc = A;
  using StateId = typename Arc::StateId;

  virtual StateId NumStates() const = 0;  // State count

  std::optional<StateId> NumStatesIfKnown() const override {
    return NumStates();
  }

  // Get a copy of this ExpandedFst. See Fst<>::Copy() for further doc.
  ExpandedFst *Copy(bool safe = false) const override = 0;

  // Read an ExpandedFst from an input stream; return NULL on error.
  static ExpandedFst *Read(std::istream &strm, const FstReadOptions &opts) {
    FstReadOptions ropts(opts);
    FstHeader hdr;
    if (ropts.header) {
      hdr = *opts.header;
    } else {
      if (!hdr.Read(strm, opts.source)) return nullptr;
      ropts.header = &hdr;
    }
    if (!(hdr.Properties() & kExpanded)) {
      LOG(ERROR) << "ExpandedFst::Read: Not an ExpandedFst: " << ropts.source;
      return nullptr;
    }
    const auto reader =
        FstRegister<Arc>::GetRegister()->GetReader(hdr.FstType());
    if (!reader) {
      LOG(ERROR) << "ExpandedFst::Read: Unknown FST type \"" << hdr.FstType()
                 << "\" (arc type = \"" << A::Type() << "\"): " << ropts.source;
      return nullptr;
    }
    auto *fst = reader(strm, ropts);
    if (!fst) return nullptr;
    return down_cast<ExpandedFst *>(fst);
  }

  // Read an ExpandedFst from a file; return NULL on error.
  // Empty source reads from standard input.
  static ExpandedFst *Read(std::string_view source) {
    if (!source.empty()) {
      std::ifstream strm(std::string(source),
                              std::ios_base::in | std::ios_base::binary);
      if (!strm) {
        LOG(ERROR) << "ExpandedFst::Read: Can't open file: " << source;
        return nullptr;
      }
      return Read(strm, FstReadOptions(source));
    } else {
      return Read(std::cin, FstReadOptions("standard input"));
    }
  }
};

namespace internal {

//  ExpandedFst<A> case - abstract methods.
template <class Arc>
inline typename Arc::Weight Final(const ExpandedFst<Arc> &fst,
                                  typename Arc::StateId s) {
  return fst.Final(s);
}

template <class Arc>
inline ssize_t NumArcs(const ExpandedFst<Arc> &fst, typename Arc::StateId s) {
  return fst.NumArcs(s);
}

template <class Arc>
inline ssize_t NumInputEpsilons(const ExpandedFst<Arc> &fst,
                                typename Arc::StateId s) {
  return fst.NumInputEpsilons(s);
}

template <class Arc>
inline ssize_t NumOutputEpsilons(const ExpandedFst<Arc> &fst,
                                 typename Arc::StateId s) {
  return fst.NumOutputEpsilons(s);
}

}  // namespace internal

// A useful alias when using StdArc.
using StdExpandedFst = ExpandedFst<StdArc>;

// This is a helper class template useful for attaching an ExpandedFst
// interface to its implementation, handling reference counting. It
// delegates to ImplToFst the handling of the Fst interface methods.
template <class I, class FST = ExpandedFst<typename I::Arc>>
class ImplToExpandedFst : public ImplToFst<I, FST> {
  using Base = ImplToFst<I, FST>;

 public:
  using Impl = I;
  using Arc = typename FST::Arc;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  StateId NumStates() const override { return GetImpl()->NumStates(); }

 protected:
  using Base::GetImpl;

  explicit ImplToExpandedFst(std::shared_ptr<Impl> impl) : Base(impl) {}

  ImplToExpandedFst(const ImplToExpandedFst &fst, bool safe)
      : Base(fst, safe) {}

  static Impl *Read(std::istream &strm, const FstReadOptions &opts) {
    return Impl::Read(strm, opts);
  }

  // Read FST implementation from a file; return NULL on error.
  // Empty source reads from standard input.
  static Impl *Read(std::string_view source) {
    if (!source.empty()) {
      std::ifstream strm(std::string(source),
                              std::ios_base::in | std::ios_base::binary);
      if (!strm) {
        LOG(ERROR) << "ExpandedFst::Read: Can't open file: " << source;
        return nullptr;
      }
      return Impl::Read(strm, FstReadOptions(source));
    } else {
      return Impl::Read(std::cin, FstReadOptions("standard input"));
    }
  }
};

// Function to return the number of states in an FST, counting them
// if necessary.
template <class Arc>
typename Arc::StateId CountStates(const Fst<Arc> &fst) {
  if (std::optional<typename Arc::StateId> num_states =
          fst.NumStatesIfKnown()) {
    return *num_states;
  } else {
    typename Arc::StateId nstates = 0;
    for (StateIterator<Fst<Arc>> siter(fst); !siter.Done(); siter.Next()) {
      ++nstates;
    }
    return nstates;
  }
}

// Function to return the number of states in a vector of FSTs, counting them if
// necessary.
template <class Arc>
typename Arc::StateId CountStates(const std::vector<const Fst<Arc> *> &fsts) {
  typename Arc::StateId nstates = 0;
  for (const auto *fst : fsts) nstates += CountStates(*fst);
  return nstates;
}

// Function to return the number of arcs in an FST.
template <class F>
size_t CountArcs(const F &fst) {
  size_t narcs = 0;
  for (StateIterator<F> siter(fst); !siter.Done(); siter.Next()) {
    narcs += fst.NumArcs(siter.Value());
  }
  return narcs;
}

}  // namespace fst

#endif  // FST_EXPANDED_FST_H_
