#pragma once

#include "shared/blueprints/ModuleDef.h"
#include "shared/blueprints/ShellDef.h"

namespace sr {

// Read-only resolution of authored ids to authored definitions.
//
// This interface exists so blueprint validation can live in sr_shared while the concrete JSON
// registries live in sr_core. Without it, validation would have to sit above the registries,
// sr_core would depend on sr_shared *and* sr_shared on sr_core, and the layer graph would have
// a cycle -- which is precisely the failure the CMake target split is meant to make impossible.
//
// It is also what lets tests validate a blueprint against a five-entry fake library instead of
// booting the real content set.
class DefLibrary {
public:
    virtual ~DefLibrary() = default;

    // Return nullptr when the id is unknown. Callers turn that into a specific, named
    // validation error -- never a silent default.
    virtual const ShellDef* FindShell(const ShellId& id) const = 0;
    virtual const ModuleDef* FindModule(const ModuleId& id) const = 0;
};

}  // namespace sr
