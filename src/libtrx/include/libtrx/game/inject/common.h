#include "../game_flow.h"
#include "./types.h"

void Inject_InitLevel(const GAME_FLOW_LEVEL *level);
void Inject_AllInjections(void);
void Inject_Cleanup(void);

extern bool Inject_IsRelevant(const INJECTION *injection);
extern bool Inject_HandleSet(const INJECTION_SET *set);

INJECTION_MESH_META Inject_GetRoomMeshMeta(int32_t room_index);
int32_t Inject_GetDataCount(INJECTION_DATA_TYPE type);
