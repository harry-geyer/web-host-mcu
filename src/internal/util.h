#pragma once

#define WHM_MAX(_a, _b)                    (_a > _b ? _a : _b)
#define WHM_MIN(_a, _b)                    (_a < _b ? _a : _b)

#define WHM_CEIL(_x, _y)                   ((_x + _y - 1) / _y)
#define WHM_ABS32(_x)                        ((uint32_t)((_x) < 0 ? -(_x) : (_x)))
