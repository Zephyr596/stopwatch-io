#include "services/VehicleJson.h"
#include <iostream>
#include <iterator>
#include <string>
int main() {
    const std::string body((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    sw::VehicleState state;
    if (!sw::decodeVehicle(body.data(), body.size(), 0, state)) return 2;
    std::cout << "source=" << unsigned(state.source) << " speed=" << state.speedKmh
              << " charging=" << state.charging << " age=" << state.dataAgeMs << '\n';
}
