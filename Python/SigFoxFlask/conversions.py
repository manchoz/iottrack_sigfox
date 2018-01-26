UINT16_t_MAX = 65536
INT16_t_MAX = 32768


def int16_to_float(value, max_val, min_val):
    conversion_factor = INT16_t_MAX / (max_val - min_val)
    return value / conversion_factor


def uint16_to_float(value, max_val, min_val):
    conversion_factor = UINT16_t_MAX / (max_val - min_val)
    return value / conversion_factor
