#include <Assets/Core/BlobReader.hpp>

namespace shuttle::assets::core {
    BlobView BlobReader::open(const std::filesystem::path& path) {
        return BlobView::open(path);
    }
}