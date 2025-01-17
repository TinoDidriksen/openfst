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
// Returns the shortest path in a (bounded-stack) PDT.

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fst/flags.h>
#include <fst/log.h>
#include <fst/extensions/pdt/pdtscript.h>
#include <fst/queue.h>
#include <fst/util.h>
#include <fst/script/fst-class.h>

DECLARE_bool(keep_parentheses);
DECLARE_string(queue_type);
DECLARE_bool(path_gc);
DECLARE_string(pdt_parentheses);

int pdtshortestpath_main(int argc, char **argv) {
  namespace s = fst::script;
  using fst::QueueType;
  using fst::ReadLabelPairs;
  using fst::script::FstClass;
  using fst::script::VectorFstClass;

  std::string usage = "Shortest path in a (bounded-stack) PDT.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.pdt [out.fst]\n";

  SET_FLAGS(usage.c_str(), &argc, &argv, true);
  if (argc > 3) {
    ShowUsage();
    return 1;
  }

  const std::string in_name =
      (argc > 1 && (strcmp(argv[1], "-") != 0)) ? argv[1] : "";
  const std::string out_name =
      (argc > 2 && (strcmp(argv[2], "-") != 0)) ? argv[2] : "";

  std::unique_ptr<FstClass> ifst(FstClass::Read(in_name));
  if (!ifst) return 1;

  if (FST_FLAGS_pdt_parentheses.empty()) {
    LOG(ERROR) << argv[0] << ": No PDT parenthesis label pairs provided";
    return 1;
  }

  std::vector<std::pair<int64_t, int64_t>> parens;
  if (!ReadLabelPairs(FST_FLAGS_pdt_parentheses, &parens)) return 1;

  VectorFstClass ofst(ifst->ArcType());

  QueueType qt;
  if (FST_FLAGS_queue_type == "fifo") {
    qt = fst::FIFO_QUEUE;
  } else if (FST_FLAGS_queue_type == "lifo") {
    qt = fst::LIFO_QUEUE;
  } else if (FST_FLAGS_queue_type == "state") {
    qt = fst::STATE_ORDER_QUEUE;
  } else {
    LOG(ERROR) << "Unknown queue type: " << FST_FLAGS_queue_type;
    return 1;
  }

  const s::PdtShortestPathOptions opts(
      qt, FST_FLAGS_keep_parentheses, FST_FLAGS_path_gc);

  s::ShortestPath(*ifst, parens, &ofst, opts);

  return !ofst.Write(out_name);
}
