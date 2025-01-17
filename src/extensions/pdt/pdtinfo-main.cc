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
// Prints out various information about a PDT such as number of states, arcs,
// and parentheses.

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fst/flags.h>
#include <fst/log.h>
#include <fst/extensions/pdt/pdtscript.h>
#include <fst/util.h>
#include <fst/script/fst-class.h>

DECLARE_string(pdt_parentheses);

int pdtinfo_main(int argc, char **argv) {
  namespace s = fst::script;
  using fst::ReadLabelPairs;
  using fst::script::FstClass;

  std::string usage = "Prints out information about a PDT.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.pdt\n";

  SET_FLAGS(usage.c_str(), &argc, &argv, true);
  if (argc > 2) {
    ShowUsage();
    return 1;
  }

  const std::string in_name =
      (argc > 1 && (strcmp(argv[1], "-") != 0)) ? argv[1] : "";

  std::unique_ptr<FstClass> ifst(FstClass::Read(in_name));
  if (!ifst) return 1;

  if (FST_FLAGS_pdt_parentheses.empty()) {
    LOG(ERROR) << argv[0] << ": No PDT parenthesis label pairs provided";
    return 1;
  }

  std::vector<std::pair<int64_t, int64_t>> parens;
  if (!ReadLabelPairs(FST_FLAGS_pdt_parentheses, &parens)) return 1;

  s::Info(*ifst, parens);

  return 0;
}
