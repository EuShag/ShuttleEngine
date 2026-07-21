#include "../src/EnvironmentBaker/EnvironmentBaker.hpp"
#include "../src/EnvironmentBlobWriter/EnvironmentBlobWriter.hpp"

int main()
{
    shuttle_engine::assets::EnvironmentBaker baker;

    auto result =
        baker.bake(R"(C:\Users\Shagu\Desktop\Bistro_v5_2\san_giuseppe_bridge_4k.hdr)");

    shuttle_engine::assets::EnvironmentBlobWriter::write(
        result,
        "studio.envb"
    );

    return 0;
}
