#pragma once
#include "HitTestTypes.hpp"
#include <vector>

namespace shuttle::pal
{
    struct TitlebarElement
    {
        Rect rect;
        WindowHitTestResult result; // CloseButton, MaximizeButton, MinimizeButton или Client (для меню)
    };

    class TitlebarLayout
    {
    public:
        // Устанавливаем общую полосу заголовка (например, Height = 40px)
        void setBounds(const Rect& bounds) noexcept { m_bounds = bounds; }
        [[nodiscard]] const Rect& getBounds() const noexcept { return m_bounds; }

        // Добавляем кнопки или интерактивные зоны (File, Edit, Search)
        void addElement(const TitlebarElement& element) { m_elements.push_back(element); }
        void clearElements() noexcept { m_elements.clear(); }

        // Кроссплатформенный алгоритм инверсивного HitTest
        [[nodiscard]] WindowHitTestResult evaluate(int32_t localX, int32_t localY) const noexcept
        {
            // 1. Если не в полосе заголовка — отказ
            if (!m_bounds.contains(localX, localY))
            {
                return WindowHitTestResult::Client;
            }

            // 2. Исключения (Кнопки закрытия, сворачивания, меню File/Edit)
            for (const auto& elem : m_elements)
            {
                if (elem.rect.contains(localX, localY))
                {
                    return elem.result; // Вернет CloseButton, MaximizeButton, Client и т.д.
                }
            }

            // 3. Всё остальное пространство в полосе — зона перетаскивания!
            return WindowHitTestResult::Caption;
        }

    private:
        Rect m_bounds{};
        std::vector<TitlebarElement> m_elements;
    };
}
