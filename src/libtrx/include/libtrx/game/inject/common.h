#include "./types.h"

void Inject_Init(const INJECTION_ARGS *args);
void Inject_AllInjections(void);
void Inject_Cleanup(void);

INJECTION_MESH_META Inject_GetRoomMeshMeta(int32_t room_index);
int32_t Inject_GetDataCount(INJECTION_DATA_TYPE type);
