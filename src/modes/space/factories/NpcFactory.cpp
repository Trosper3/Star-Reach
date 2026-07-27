#include "modes/space/factories/NpcFactory.h"

namespace sr::space::npc_factory {

SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params) {
    return rig_factory::Spawn(world, content, params);
}

}  // namespace sr::space::npc_factory
