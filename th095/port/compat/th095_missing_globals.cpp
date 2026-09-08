// th095 port layer: definitions for the production global variables that the
// upstream reconstruction (N0zoM1z0/th095, pinned b864c31) declares (mostly
// via per-TU `extern` lines and diffbuild.hpp DIFFABLE_* macros) but never
// defines. The upstream VC7.1 link is documented as "failing closed" on this
// contract. The port supplies the definitions so the executable can link.
//
// The valueless globals are zero/NULL initialised. Globals whose original
// data is known from context are filled with that data; the rest are marked
// TODO(port) and need disassembly-level extraction from th095.exe 1.02a.
#include <windows.h>
#include <d3d8.h>
#include "inttypes.hpp"
#include "ZunResult.hpp"
#include "AnmVmId.hpp"
namespace th095
{

struct AnmVm;
struct Enemy;
struct EclRawInstruction;
struct EnemyEclInterpolationSlot;

// --- type definitions copied from the upstream sources (same TUs declare
// --- local copies; this TU owns the storage) ---


// Main.hpp (packed like the original)
#pragma pack(push, 2)
struct ControllerBinding
{
    u32 inputs[4];
    u16 button;
};
struct ControllerMapping
{
    ControllerBinding primaryBindings[3];
    u8 unknown036[0x58];
    ControllerBinding secondaryBindings[3];
};
#pragma pack(pop)

// OptionsMenu.hpp
struct OptionsControllerBinding
{
    i16 button00;
    i16 button02;
    i16 unknown04;
    i16 button06;
    u8 unknown08[0x0a];
};
struct OptionsGameConfigView
{
    OptionsControllerBinding controllerBinding;
    u8 unknown012[0x9d];
    u8 windowed;
    u8 unknown0b0[5];
    i8 bgmVolume;
    i8 sfxVolume;
};
struct OptionsControllerMappingView
{
    OptionsControllerBinding primaryBinding;
};

// ReplayBrowser.hpp
struct ReplayBrowserExitSignal
{
    u8 unknown000[8];
    i32 requested;
    void Request();
};

// ReplayInputSource.hpp
struct ReplayInputSource
{
    u16 currentInput;
    u16 unknown002;
    u16 repeatOutput;
    u16 pressedInput;
    u8 unknown008[0x24];
    u16 historyCurrent;
    u16 historyPrevious;
    u16 historyRepeat;
    u16 historyPressed;
    u16 historyReleased;
    u16 unknown036;
    u16 heldFrames[16];
    void Update();
};

// pbg/PbgArchive.cpp
struct PbgDecryptProfile
{
    u8 xorValue;
    u8 xorValueIncrement;
    u8 unknown02[2];
    i32 chunkSize;
    i32 maxBytes;
};

// PhotoGameTask.cpp
struct PhotoReplayInputButtonsTaskView
{
    u16 current;
    u16 operator&(u16 mask) { return this->current & mask; }
};

// Main.cpp
struct SupervisorInputWorkerView
{
    void Start(void (__fastcall *callback)(void *), void *argument);
    void Stop();
};

// SoundPlayer.hpp
struct SoundBufferIdxVolume
{
    i32 bufferIdx;
    i16 volume;
    i16 unconsumedMetadata;
};

// AnmManager.hpp
struct VertexTex1Xyzrhw
{
    f32 x, y, z, w, u, v;
};

// ecl/EclRunHigh.inl
typedef void (__fastcall *Th095ExInsn)(Enemy *, EclRawInstruction *);
// ecl/EclManager.hpp
typedef void (__fastcall *EnemyEclInterpolatorCallback)(
    Enemy *enemy, EnemyEclInterpolationSlot *slot, f32 progress);

// --- namespace EclExtended globals (EclExtended.cpp declares these) ---
namespace EclExtended
{
struct AnmManager;
struct AnmBackgroundStateDrawView;
struct Background;
struct ExtendedPhotoEnemyManagerView;
struct ExtendedRuntimeView;
struct ExtendedBulletManager;
struct PhotoBulletManagerView;
struct ExtendedPhotoEffectManager;
struct PhotoEffectManagerView;
struct ExtendedPlayerView;

struct AnmManager *g_AnmManager;                          // TODO(port): NULL until AnmManager is constructed
struct Background *g_Background;                          // TODO(port)
struct ExtendedPhotoEnemyManagerView *g_ExtendedPhotoEnemyManager; // TODO(port)
struct ExtendedRuntimeView *g_ExtendedRuntime;            // TODO(port)
struct ExtendedBulletManager *g_PhotoBulletManager;       // TODO(port)
struct ExtendedPhotoEffectManager *g_PhotoEffectManager;  // TODO(port)
struct ExtendedPlayerView *g_Player;                      // TODO(port)
} // namespace EclExtended

// --- namespace EclRunHigh globals (EclRunHigh.inl declares these) ---
namespace EclRunHigh
{
struct AnmManagerLookup;
struct Th095BulletManager;
struct PhotoCamera;
struct PhotoEffectManager;
struct Th095StageController;
struct PhotoModeController;

u8 *g_Th095StageState;                                    // TODO(port): runtime state block base
AnmManagerLookup *g_Th095AnmManager;                      // TODO(port)
PhotoCamera *g_Th095PhotoCamera;                          // TODO(port)
Th095BulletManager *g_Th095BulletManager;                 // TODO(port)
Th095StageController *g_Th095StageController;             // TODO(port)
PhotoEffectManager *g_Th095PhotoEffectManager;            // TODO(port)
PhotoModeController *g_Th095PhotoMode;                    // TODO(port)
u8 *g_Th095Runtime;                                       // TODO(port)
Th095ExInsn g_Th095ExInsn[256];                           // TODO(port): extended ECL instruction table (opcode-indexed)
} // namespace EclRunHigh

// --- namespace EclRunLow globals (EclRunLow.inl / EclDependencies.cpp) ---
namespace EclRunLow
{
struct Player;
Player *g_Th095Player;                                    // TODO(port)
EnemyEclInterpolatorCallback g_EclInterpolatorCallbacks[7]; // TODO(port): ANM interpolation callbacks (ANM_INTERP_*)
} // namespace EclRunLow

// --- plain th095 globals ---

char g_WindowTitle[] = "\xe6\x9d\xb1\xe6\x96\xb9\xe6\x96\x87\xe8\x8a\xb1\xe8\xb3\xbc \xe2\x81\xbd Shoot the Bullet"; // 東方文花帖 〜 Shoot the Bullet
char g_ReplayPath[0x100];
char g_SelectedReplayPath[0x100];
HANDLE g_ExclusiveMutex;                                  // TODO(port): create the real mutex at startup
i32 g_MusicArchiveBaseOffset;
i32 g_DemoReplayIndex;
u32 g_FrontEndConfigurationFlags;
i32 g_FrontEndLoadActive;
i32 g_FrontEndUiState;
u16 g_FrontEndCurrentInput;
i32 g_SoundInitializationComplete;

struct FrontEndGameManagerView;
struct FrontEndControllerView;
struct AsciiStageStateView;
struct BackgroundRuntimeView;
struct BackgroundStageStateView;
struct EclFloatLValuePlayerView;
struct EclFloatLValueRuntimeView;
struct EclFloatOperandPlayerView;
struct EclFloatOperandRuntimeView;
struct EclIntLValueRuntimeView;
struct EclOperandPlayerView;
struct EclOperandRuntimeView;
struct EnemyShotBulletManagerView;
struct OptionsMenuView;
struct PhotoItemManagerView;
struct PhotoBulletPlayerView;
struct PhotoBulletManagerView;
struct PhotoCaptureParticleSpawnerView;
struct PhotoCardInfoView;
struct PhotoCardGameRuntimeView;
struct PhotoCardStageStateView;
struct PhotoEnemyBulletManagerView;
struct PhotoEnemyGameView;
struct PhotoEnemyPlayerView;
struct PhotoEnemyManagerTaskView;
struct PhotoEffectManagerView;
struct PhotoFrontManagerView;
struct PhotoFrontRuntimeView;
struct PhotoFrontStageStateView;
struct PhotoGameTaskView;
struct PhotoGameRuntimeTaskView;
struct PhotoRuntimeView;
struct PhotoStageBulletManagerView;
struct PhotoStageControllerView;
struct PhotoStageEffectManagerView;
struct PhotoStageRuntimeView;
struct PhotoStageStateView;
struct PhotoStageSupervisorView;
struct PhotoResetTargetView;
struct PhotoGameStageStateView;
struct PhotoGameTaskDrawGateView;
struct PhotoBulletManagerTaskView;
struct PhotoStageStateTaskView;
struct PhotoReplayInputButtonsTaskView;
struct ReplayManager;
struct ResultScreen;
struct ResultPhotoControllerView;
struct ResultPhotoDataView;
struct ScorePhotoStageView;
struct ScreenEffect;
struct AnmLoaded;

FrontEndGameManagerView *g_FrontEndGameManager;           // TODO(port)
AsciiStageStateView *g_AsciiStageState;                   // TODO(port)
BackgroundRuntimeView *g_BackgroundRuntime;               // TODO(port)
BackgroundStageStateView *g_BackgroundStageState;         // TODO(port)
EclFloatLValuePlayerView *g_EclFloatLValuePlayer;         // TODO(port)
EclFloatLValueRuntimeView *g_EclFloatLValueRuntime;       // TODO(port)
EclFloatOperandPlayerView *g_EclFloatOperandPlayer;       // TODO(port)
EclFloatOperandRuntimeView *g_EclFloatOperandRuntime;     // TODO(port)
EclIntLValueRuntimeView *g_EclIntLValueRuntime;           // TODO(port)
EclOperandPlayerView *g_EclOperandPlayer;                 // TODO(port)
EclOperandRuntimeView *g_EclOperandRuntime;               // TODO(port)
EnemyShotBulletManagerView *g_EnemyShotBulletManager;     // TODO(port)
u8 *g_EnemyShotPlayer;                                    // TODO(port)
PhotoItemManagerView *g_ItemManager;                      // TODO(port)
PhotoBulletPlayerView *g_PhotoBulletPlayer;               // TODO(port)
PhotoBulletManagerView *g_PhotoBulletManager;             // TODO(port)
PhotoResetTargetView *g_PhotoBulletResetTarget;           // TODO(port)
PhotoCaptureParticleSpawnerView *g_PhotoCaptureParticleSpawner; // TODO(port)
PhotoEffectManagerView *g_PhotoEffectManager;                // TODO(port) (plain th095:: symbol, PhotoEffect.cpp)
AnmLoaded *g_PhotoCardBackgroundAnm;                      // TODO(port)
PhotoCardInfoView *g_PhotoCardInfo;                       // TODO(port)
AnmLoaded *g_PhotoCardUiAnm;                              // TODO(port)
PhotoCardGameRuntimeView *g_PhotoCardGameRuntime;         // TODO(port)
PhotoCardStageStateView *g_PhotoCardStageState;           // TODO(port)
PhotoEnemyBulletManagerView *g_PhotoEnemyBulletManager;   // TODO(port)
PhotoEnemyGameView *g_PhotoEnemyGame;                     // TODO(port)
struct PhotoEnemyManagerView *g_PhotoEnemyManager;        // TODO(port)
PhotoEnemyPlayerView *g_PhotoEnemyPlayer;                 // TODO(port)
PhotoEnemyManagerTaskView *g_PhotoEnemyManagerTask;       // TODO(port)
PhotoFrontManagerView *g_PhotoFrontManager;               // TODO(port)
PhotoFrontRuntimeView *g_PhotoFrontRuntime;               // TODO(port)
PhotoFrontStageStateView *g_PhotoFrontStageState;         // TODO(port)
struct PhotoGameStateView *g_PhotoGame;                   // TODO(port)
PhotoGameTaskView *g_PhotoGameTask;                       // TODO(port)
PhotoGameRuntimeTaskView *g_PhotoGameRuntime;             // TODO(port)
PhotoRuntimeView *g_PhotoRuntime;                         // TODO(port)
PhotoStageBulletManagerView *g_PhotoStageBulletManager;   // TODO(port)
PhotoStageControllerView *g_PhotoStageController;         // TODO(port)
PhotoStageEffectManagerView *g_PhotoStageEffectManager;   // TODO(port)
PhotoStageRuntimeView *g_PhotoStageRuntime;               // TODO(port)
PhotoStageStateView *g_PhotoStageState;                   // TODO(port)
PhotoStageSupervisorView *g_PhotoStageSupervisor;         // TODO(port)
PhotoResetTargetView *g_PhotoStageResetTarget;            // TODO(port)
PhotoResetTargetView *g_PhotoRuntimeResetTarget;          // TODO(port)
PhotoGameStageStateView *g_PhotoStageStateForPlayer;      // TODO(port)
ReplayManager *g_ReplayManager;                           // TODO(port)
ResultScreen *g_ResultScreen;                             // TODO(port)
ResultPhotoControllerView *g_ResultPhotoController;       // TODO(port)
ResultPhotoDataView *g_ResultPhotoData;                   // TODO(port)
ScorePhotoStageView *g_ScorePhotoStage;                   // TODO(port)
ScreenEffect *g_SupervisorScreenEffect;                   // TODO(port)

ControllerMapping g_ControllerMapping;                    // TODO(port): default bindings
OptionsGameConfigView g_OptionsGameConfig;                // TODO(port)
OptionsControllerMappingView g_OptionsControllerMapping;  // TODO(port)
i16 g_OptionsLastJoystickButton;
ReplayBrowserExitSignal g_ReplayBrowserExitSignal;
i32 g_ReplayBrowserSelection;
ReplayInputSource g_ReplayInputSource;
PhotoReplayInputButtonsTaskView g_ReplayInputButtons;
u16 g_ReplayInputAux;
u16 g_ReplayInputFlags;
SupervisorInputWorkerView g_SupervisorInputWorker;
AnmVmId g_SupervisorLoadingVms[3];
u8 g_SupervisorLoadingVmsPad[0]; // (no-op, keeps layout comments honest)

PbgDecryptProfile g_PbgDecryptProfiles[8];                // TODO(port): archive decryption profiles
char *g_PbgFileOpenModes[3] = { "rb", "wb", "ab" };       // TODO(port): verify against original
i32 g_PbgFileSeekModes[3] = { 0, 1, 2 };                  // SEEK_SET / SEEK_CUR / SEEK_END

D3DFORMAT g_TextureFormatD3D8Mapping[6] = {               // TODO(port): verify format table order
    D3DFMT_DXT1, D3DFMT_A8R8G8B8, D3DFMT_X1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_R5G6B5, D3DFMT_A8
};
u32 g_TextureFormatBytesPerPixel[6] = { 2, 4, 2, 2, 2, 1 };

VertexTex1Xyzrhw g_BackgroundQuadVertices[4];             // TODO(port): background quad corners
u32 g_PhotoAsciiTextColor;
f32 g_PhotoBulletCollisionSizes[32];                      // TODO(port)
i32 g_PhotoBulletDrawBucketIndices[32];                   // TODO(port)
u32 g_PhotoBulletColors4[4];                              // TODO(port)
u32 g_PhotoBulletColors8[8];                              // TODO(port)
u32 g_PhotoBulletColors16[16];                            // TODO(port)
i32 g_PhotoBulletScriptBases[32];                         // TODO(port)
u8 g_PhotoCaptureCountdown;
u16 g_PhotoInput;
u16 g_PhotoInputPressed;
i32 g_PhotoLoadWaitFlag;
i32 g_PhotoNextState;
PhotoItemManagerView *g_PhotoItemManager;
// PhotoGameTask.cpp
struct PhotoRuntimeConfigView
{
    u32 values[50];
    PhotoRuntimeConfigView() { this->Initialize(); }
    void Initialize();
};
PhotoRuntimeConfigView g_PhotoRuntimeConfig;              // TODO(port)
PhotoBulletManagerTaskView *g_PhotoBulletManagerTask;
struct PhotoLaserManagerTaskView;
struct PhotoPlayerManagerView;
PhotoLaserManagerTaskView *g_PhotoLaserManagerTask;       // TODO(port)
PhotoPlayerManagerView *g_PhotoPlayerManager;             // TODO(port)

u32 g_SceneGroupColors[11];                               // TODO(port)
u32 g_SceneLockedTransitionColor;
u32 g_SceneLockedInitialColor;
u8 g_SceneTextBuffer[0x40];
i32 g_ResultSceneState;
i32 g_ResultGroupMap[32];                                 // TODO(port)
i32 g_ResultSceneLimits[32];                              // TODO(port)
const char *g_ResultAlphabet;                             // TODO(port)
SoundBufferIdxVolume g_SoundBufferIdxVol[47];             // TODO(port)
char *g_SFXList[37];                                      // TODO(port): SFX file name table
u32 g_PhotoEffectColors[32];                              // TODO(port)

} // namespace th095
