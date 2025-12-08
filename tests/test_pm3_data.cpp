#include <type_traits>

#include "pm3_data.h"

int main() {
    static_assert(std::is_standard_layout_v<gamea>, "gamea must stay standard layout");
    static_assert(std::is_standard_layout_v<gameb>, "gameb must stay standard layout");
    static_assert(std::is_standard_layout_v<gamec>, "gamec must stay standard layout");
    static_assert(std::is_standard_layout_v<ClubRecord>, "ClubRecord must stay standard layout");
    static_assert(std::is_standard_layout_v<PlayerRecord>, "PlayerRecord must stay standard layout");

    static_assert(sizeof(gamea) == 29554, "gamea size must match PM3 binary");
    static_assert(sizeof(gameb) == 139080, "gameb size must match PM3 binary");
    static_assert(sizeof(gamec) == 157280, "gamec size must match PM3 binary");
    static_assert(sizeof(ClubRecord) == 570, "ClubRecord size must match PM3 binary");
    static_assert(sizeof(PlayerRecord) == 40, "PlayerRecord size must match PM3 binary");
    return 0;
}
