#include "../src/EnvironmentBaker/EnvironmentBaker.hpp"
#include "../src/EnvironmentBlobWriter/EnvironmentBlobWriter.hpp"

int main()
{
    shuttle_engine::assets::EnvironmentBaker baker;

    auto result =
        baker.bake(R"(C:\Users\Shagu\Downloads\qwantani_sunset_puresky_4k (1).hdr)");

    shuttle_engine::assets::EnvironmentBlobWriter::write(
        result,
        "studio.envb"
    );

    return 0;
}
