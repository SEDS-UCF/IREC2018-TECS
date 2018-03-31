#ifndef COMMON_H
#define COMMON_H

#define NETWORK_PORT 1963

// ERROR CODES
#define ERR_MPU_MAIN_NULL 0
#define ERR_MPU_MAIN_INIT_FAIL 0
#define ERR_BARO_INIT_FAIL 0
#define ERR_BARO_NULL 0
#define ERR_MPU_AUX_NULL 0
#define ERR_MPU_AUX_INIT_FAIL 0
#define ERR_BCM_INIT_FAIL 0

// Yes I know this is bad practice. Yes I know this could fail terribly. I'm doing it anyways.
// We're only working with single files here.
// If each linked executable had multiple translation units, then fine, this would be bad.
// But they don't. So we're putting functions in header files. Deal with it. <3

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

#endif //COMMON_H