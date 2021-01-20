#ifndef SHARED_WORLD_BLOCK_BLOCKIDS_H
#define SHARED_WORLD_BLOCK_BLOCKIDS_H

#include <uuid.h>

namespace world {
/// ID of the air block
const uuids::uuid kAirBlockId = uuids::uuid::from_string("714a92e3-2984-4f0e-869e-14162d462760");

namespace blocks {
const auto kDirtBlockId = uuids::uuid::from_string("2be68612-133b-40c6-8436-189d4bd87a4e");
const auto kStoneBlockId = uuids::uuid::from_string("27D25383-4466-405D-9DEE-1FCF4A6272CC");
}
};

#endif
