#include <iostream>
#include <cstdint>
#include <bitset>

void pack_int(uint64_t& box, int64_t value, size_t size) {
	box <<= size;
	box |= (uint64_t)value & (0xFFFFFFFFFFFFFFFF >> (64 - size));
}

int64_t unpack_int(uint64_t& box, size_t size, bool retsigned = true) {
	if(size == 0)
		return 0;

	uint64_t ret = box & (0xFFFFFFFFFFFFFFFF << (64 - size));
	ret >>= (64 - size);
	
	box <<= size;

	if(retsigned) {
		bool negative = (ret & (1 << (size - 1))) != 0;
		if(negative)
			ret |= 0xFFFFFFFFFFFFFFFF << size;

		return (int64_t)ret;
	}
	
	return ret;
}

int main() {

    uint64_t group1 = 0;

    while(true) {
		int64_t in_value, in_size;
		std::cout << "p: ";
		std::cin >> in_value >> in_size;

		pack_int(group1, in_value, in_size);
		std::cout << std::bitset<64>(group1) << std::endl;

		std::cout << "u: ";
		std::cin >> in_size;

		std::cout << unpack_int(group1, in_size) << std::endl;
		std::cout << std::bitset<64>(group1) << std::endl;
	}
}