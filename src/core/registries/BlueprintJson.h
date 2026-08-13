#pragma once

#include "core/registries/JsonReader.h"
#include "shared/blueprints/MaterialDef.h"
#include "shared/blueprints/ModuleDef.h"
#include "shared/blueprints/ShellDef.h"
#include "shared/blueprints/ShipBlueprint.h"

// The parser boundary.
//
// This header is the only place authored JSON becomes a C++ blueprint struct. Law 10 says JSON
// in data/ is the single source of authored content; tools/ci/check_content_pipeline.py enforces
// that by rejecting ModuleDef/ShellDef/ShipBlueprint literals anywhere outside this directory
// and tests/. StarReach2 ran a JSON registry and 883 lines of hardcoded C++ factories side by
// side for three years because nothing declared which one won. This declares it.
namespace sr::core {

ModuleDef ParseModuleDef(const JsonReader& reader);
ShellDef ParseShellDef(const JsonReader& reader);
ShipBlueprint ParseShipBlueprint(const JsonReader& reader);
MaterialDef ParseMaterialDef(const JsonReader& reader);

}  // namespace sr::core
