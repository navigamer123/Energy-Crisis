// =============================================================================
// Team b-session (HX-15): projector picture (see UI_postfx.h)
// =============================================================================
#include "../includes/UI_postfx.h"
#include "../includes/UI_settings.h"
#include <iostream>

namespace {
// Tuned on screenshots: night-time indigo stays readable, gold/cyan text pops, whites do not clip
constexpr float PROJECTOR_GAMMA = 0.80f;      // < 1 lifts the dark tones projectors swallow
constexpr float PROJECTOR_CONTRAST = 1.18f;
constexpr float PROJECTOR_SATURATION = 1.22f;

const char* kProjectorShader = R"(
uniform sampler2D source;
uniform float gamma;
uniform float contrast;
uniform float saturation;
void main() {
    vec4 c = texture2D(source, gl_TexCoord[0].xy);
    vec3 col = pow(max(c.rgb, vec3(0.0)), vec3(gamma));
    col = (col - 0.5) * contrast + 0.5;
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(l), col, saturation);
    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)";
} // namespace

bool UI_postfx::isSupported() {
    if (!tried) {
        tried = true;
        ok = sf::Shader::isAvailable() && shader.loadFromMemory(kProjectorShader, sf::Shader::Type::Fragment);
        if (ok) {
            shader.setUniform("gamma", PROJECTOR_GAMMA);
            shader.setUniform("contrast", PROJECTOR_CONTRAST);
            shader.setUniform("saturation", PROJECTOR_SATURATION);
        } else {
            std::cerr << "[UI_postfx] Shaders unavailable: projector mode keeps the normal picture.\n";
        }
    }
    return ok;
}

void UI_postfx::apply(sf::RenderWindow& window) {
    if (!gameSettings().projectorMode || !isSupported()) return;
    sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0) return;
    if (frame.getSize() != size && !frame.resize(size)) return;
    frame.update(window);

    sf::View previous = window.getView();
    window.setView(sf::View(sf::FloatRect({ 0.0f, 0.0f }, { static_cast<float>(size.x), static_cast<float>(size.y) })));
    sf::Sprite sprite(frame);
    sf::RenderStates states;
    states.shader = &shader;
    states.blendMode = sf::BlendNone;
    shader.setUniform("source", sf::Shader::CurrentTexture);
    window.draw(sprite, states);
    window.setView(previous);
}
