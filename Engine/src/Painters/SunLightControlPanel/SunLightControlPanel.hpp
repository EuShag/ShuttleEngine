//
// Created by Shagu on 15.06.2026.
//

#ifndef HELLOTRIANGLE_SUNLIGHTCONTROLPANEL_HPP
#define HELLOTRIANGLE_SUNLIGHTCONTROLPANEL_HPP
#include <glm/glm.hpp>

#include "Render/uiPass.hpp"

namespace shuttle
{
class SunLightControlPanel : public engine::render::IuiPainter
{
  public:
    // Передаем ссылки на данные, которыми будем управлять
    SunLightControlPanel(glm::vec4& dir, glm::vec4& color, float& intensity)
        : m_dir(dir), m_color(color), m_intensity(intensity)
    {
    }

    void drawUi() override
    {
        ImGui::Begin("SunLight Control");

        // Поворот солнца
        ImGui::SliderFloat3("Direction", &m_dir.x, -1.0f, 1.0f);

        // Цвет и интенсивность
        ImGui::ColorEdit4("Color", &m_color.x);
        ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 20.0f);

        ImGui::End();
    }

  private:
    // Ссылки на реальные переменные в движке
    glm::vec4& m_dir;
    glm::vec4& m_color;
    float& m_intensity;
};

} // namespace shuttle

#endif // HELLOTRIANGLE_SUNLIGHTCONTROLPANEL_HPP
