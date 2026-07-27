#pragma once

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system (or, here, modes/space/render/) under modes/space/.
namespace sr {

// Tag + falloff range: this entity is a light source for LightingPass's per-object brightness
// falloff (modes/space/render/LightingPass.h). WorldGen's sun is the only light source any
// system generates today -- a binary system or an artificial light later just adds a second
// entity carrying this component; LightingPass does not care how many exist, it takes the
// brightest applicable one per object.
struct LightSource {
    float maxRange = 6000.0f;  // World units. Attenuation reaches the ambient floor at this range.
};

}  // namespace sr
