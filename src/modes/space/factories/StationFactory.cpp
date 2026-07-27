#include "modes/space/factories/StationFactory.h"

namespace sr::space::station_factory {

SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params) {
    const ShipBlueprint* blueprint = content.FindShip(params.blueprint);
    if (blueprint == nullptr || blueprint->mobile) {
        return {};
    }
    return rig_factory::Spawn(world, content, params);
}

}  // namespace sr::space::station_factory
