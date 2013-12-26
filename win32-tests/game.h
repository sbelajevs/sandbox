#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GameAPI;
struct SysAPI;

GameAPI* GameAPI_Create();
void GameAPI_Init(GameAPI* game, SysAPI* sys, int w, int h, float frameTime);
void GameAPI_Update(GameAPI* game);
void GameAPI_Render(GameAPI* game);
void GameAPI_Resize(GameAPI* game, int w, int h);
void GameAPI_OnClosing(GameAPI* game);
int  GameAPI_Finished(GameAPI* game);
void GameAPI_Release(GameAPI* game);

#ifdef __cplusplus
}
#endif
