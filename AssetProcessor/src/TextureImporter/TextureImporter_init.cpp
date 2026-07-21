#include "TextureImporter.hpp"
#include "bc7enc.h"
#include "rgbcx.h"

namespace shuttle_engine::compiler {
    void TextureImporter::initialize() {
        // Инициализация внешних библиотек; функции idempotent
        bc7enc_compress_block_init();
        rgbcx::init();
    }
}
