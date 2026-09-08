// th095 port layer: definitions for production symbols that the
// upstream reconstruction (N0zoM1z0/th095, pinned b864c31) declares
// but does not define. The upstream VC7.1 link is documented as
// "failing closed" on exactly this contract (README: "the real
// VC7.1 link exposed unresolved production type/global ownership").
//
// The port supplies these definitions so the executable can link.
// Bodies marked TODO(port) are minimal safe placeholders; correct
// behaviour requires disassembly-level fidelity work against the
// original th095.exe 1.02a (see ATTRIBUTION.md).
#include <string.h>
#include "inttypes.hpp"
#include "ZunMath.hpp"
#include "ZunResult.hpp"
#include "ZunBool.hpp"

namespace th095
{

// --- minimal type shells (only referenced through pointers) ---
struct AngleFromPoint;
struct AngleToPoint;
struct AnmVm;
struct AnmVmListNode;
struct Begin;
struct BeginPhotoCapture;
struct CSoundManager;
struct CallEclSub;
struct CapturePhotoPixels;
struct CapturePhotoTargets;
struct CheckBulletCollision;
struct CheckIfFileAlreadyExists;
struct ClampPosition;
struct Clear;
struct ClearCapturedBullets;
struct CommitCapturedObjects;
struct ConfigureEnemyPhotoAnm;
struct CountNearbyTargets;
struct CountPhotoTargets;
struct Create;
struct CreateFrontEndGameManager;
struct CreateVmAtWorld;
struct CreateVmAtWorldInto;
struct Destroy;
struct DispatchExtendedValue;
struct Draw;
// ecl/EclManager.hpp
union EclRawOperand
{
    i32 asInt;
    f32 asFloat;
};
struct End;
struct EnemyEclContext;
struct EnemyShotDescriptorView;
struct ExecuteScript;
struct ExtendedVmHandle;
struct FindVm;
struct Finish;
struct FromAngleMagnitude;
struct FrontEndControllerView;
struct FrontEndGameManagerView;
struct GetAngle;
struct GetInput;
struct GetPhotoBulletScriptBase;
struct GetPhotoEffectScriptBase;
struct GetVm;
struct Initialize;
struct InitializeVm;
struct LoadResources;
struct MarkVmForDeletion;
struct OnDraw;
struct OnUpdate;
struct PhotoBackgroundManagerView;
struct PhotoBulletManagerTaskView;
struct PhotoBulletVector;
struct PhotoEnemyManagerTaskView;
struct PhotoEnemyView;
struct PhotoItemManagerTaskView;
struct PhotoLaserManagerTaskView;
struct PhotoPlayerManagerView;
struct PhotoSessionDescriptor;
struct PhotoToScreen;
struct Pop;
struct PreparePhotoResultScreen;
struct ReadFileData;
struct ReleaseReplayAnm;
struct ReleaseSceneSelectAnms;
struct RemoveVm;
struct RemoveVmListNode;
struct ReplaceActive;
struct ReplayManagerTaskView;
struct ResetEnemies;
struct ResetEnemyPatterns;
struct ResetEnemyState;
struct ResetForPhotoTransition;
struct ResolveFloat;
struct RestartPhotoTargetEcls;
struct ResultScreen;
struct RotatePhotoEffectVector;
struct RotatePhotoStagePoint;
struct RunEcl;
struct SetColor1Interpolation;
struct SetInterrupt;
struct SetPosition;
struct SetSprite;
struct SetVmInterrupt;
struct SetVmPosition;
struct Spawn;
struct SpawnBulletPattern;
struct SpawnEnemy;
struct SpawnEnemyPattern;
struct SpawnPhotoStageEffect;
struct Start;
struct Stop;
struct Tick;
struct Update;

// AnmVmId: the real type (AnmVmId.hpp) is a 4-byte struct wrapping an i32.
struct AnmVmId
{
    i32 value;
    AnmVmId() : value(0) {}
    AnmVmId(i32 v) : value(v) {}
};

struct AnmManager;
struct AnmVm;
struct CSoundManager;
struct EclFloatOperandPlayerView;
struct EclManager;
struct EclOperandPlayerView;
struct Enemy;
struct EnemyShotBulletManagerView;
struct FrontEndControllerView;
struct FrontEndLifecycleView;
struct FrontEndPointerQueueView;
struct HelpTextureEntryView;
struct MidiOutput;
struct MusicRoomTextureEntryView;
struct PhotoAnmManagerView;
struct PhotoBackgroundManagerView;
struct PhotoBulletManagerTaskView;
struct PhotoBulletManagerView;
struct PhotoBulletPlayerView;
struct PhotoBulletVector;
struct PhotoCaptureParticleSpawnerView;
struct PhotoEnemyEclManagerView;
struct PhotoEnemyManagerTaskView;
struct PhotoEnemyPlayerView;
struct PhotoGameFileSystemView;
struct PhotoItemManagerTaskView;
struct PhotoLaserManagerTaskView;
struct PhotoPlayerManagerView;
struct PhotoResetTargetView;
struct PhotoRuntimeConfigView;
struct PhotoStageAnmManagerView;
struct PhotoStageBulletManagerView;
struct PhotoStageControllerView;
struct PhotoStageEffectManagerView;
struct Player;
struct ReplayManagerTaskView;
struct ResultAnmVmDrawView;
struct SceneSelectColorInterpolationView;
struct ScorePhotoStageView;
struct ScreenEffectTimer;
struct SupervisorControllerView;
struct SupervisorInputWorkerView;

namespace EclExtended
{
struct AnmManagerLookupView;
struct ExtendedAnmSpawner;
struct ExtendedPhotoCameraView;
struct ExtendedPhotoEffectManager;
struct ExtendedPhotoEnemyManagerView;
struct ExtendedVector;
struct ExtendedVmHandle;
} // namespace EclExtended

namespace EclRunHigh
{
struct AnmManagerLookup;
struct EnemyFloatOperandView;
struct PhotoAnmSpawner;
struct PhotoCamera;
struct PhotoEffectManager;
struct PhotoModeController;
struct PhotoSession;
struct PhotoSessionDescriptor;
struct Th095BulletManager;
struct Th095RuntimeManager;
struct Th095StageController;
} // namespace EclRunHigh

struct AnmManager
{
    i32 RemoveVmListNode(th095::AnmVmListNode*);
};

struct AnmVm
{
    AnmVm();
    ~AnmVm();
};

struct CSoundManager
{
    CSoundManager();
};

struct EclFloatOperandPlayerView
{
    f32 AngleFromPoint(th095::Float3*);
};

struct EclManager
{
    ZunResult CallEclSub(th095::EnemyEclContext*, short);
};

struct EclOperandPlayerView
{
    f32 AngleFromPoint(th095::Float3*);
};

struct Enemy
{
    void ClampPosition();
};

struct EnemyShotBulletManagerView
{
    i32 SpawnBulletPattern(th095::EnemyShotDescriptorView*);
};

struct FrontEndControllerView
{
    FrontEndControllerView * Create(int);
    void Destroy();
};

struct FrontEndLifecycleView
{
    void OnDraw(void*);
    i32 OnUpdate(void*);
};

struct FrontEndPointerQueueView
{
    i32 Pop();
};

struct HelpTextureEntryView
{
    void Clear();
};

struct MidiOutput
{
    i32 ReadFileData(int, char*);
};

struct MusicRoomTextureEntryView
{
    void Clear();
};

struct PhotoAnmManagerView
{
    AnmVm * FindVm(int);
    void RemoveVm(int);
    void SetVmInterrupt(int, short);
    void SetVmPosition(int, th095::Float3 const*);
};

struct PhotoBackgroundManagerView
{
    PhotoBackgroundManagerView * Create();
    void Destroy();
};

struct PhotoBulletManagerTaskView
{
    PhotoBulletManagerTaskView * Create();
    void Destroy();
};

struct PhotoBulletManagerView
{
    void BeginPhotoCapture(th095::Float3 const*, th095::Float3 const*);
    ZunResult CapturePhotoTargets(th095::Float3 const*, th095::Float3 const*);
    i32 CountNearbyTargets(th095::Float3 const*, float);
};

struct PhotoBulletPlayerView
{
    f32 AngleFromPoint(th095::PhotoBulletVector*);
    i32 CheckBulletCollision(th095::PhotoBulletVector*, th095::PhotoBulletVector*);
};

struct PhotoBulletVector
{
    void FromAngleMagnitude(float, float);
};

struct PhotoCaptureParticleSpawnerView
{
    i32 Spawn(int, th095::Float3*, unsigned int);
};

struct PhotoEnemyEclManagerView
{
    i32 RunEcl(th095::PhotoEnemyView*);
};

struct PhotoEnemyManagerTaskView
{
    void Destroy();
    i32 LoadResources();
    void RestartPhotoTargetEcls();
    i32 Update();
    PhotoEnemyManagerTaskView();
    ~PhotoEnemyManagerTaskView();
};

struct PhotoEnemyPlayerView
{
    i32 CheckBulletCollision(th095::Float3*, th095::Float3*);
};

struct PhotoGameFileSystemView
{
    BOOL CheckIfFileAlreadyExists(char const*);
};

struct PhotoItemManagerTaskView
{
    PhotoItemManagerTaskView * Create();
    void Destroy();
};

struct PhotoLaserManagerTaskView
{
    PhotoLaserManagerTaskView * Create();
    void Destroy();
};

struct PhotoPlayerManagerView
{
    PhotoPlayerManagerView * Create();
    void Destroy();
};

struct PhotoResetTargetView
{
    void ResetForPhotoTransition();
};

struct PhotoRuntimeConfigView
{
    void Initialize();
};

struct PhotoStageAnmManagerView
{
    AnmVm * GetVm(int);
    void MarkVmForDeletion(int);
    void SetInterrupt(int, int);
    void SetPosition(int, th095::Float3*);
};

struct PhotoStageBulletManagerView
{
    i32 ClearCapturedBullets();
};

struct PhotoStageControllerView
{
    i32 CountNearbyTargets(th095::Float3 const*, float);
    i32 CountPhotoTargets(th095::Float3 const*, th095::Float3 const*);
};

struct PhotoStageEffectManagerView
{
    i32 CommitCapturedObjects();
};

struct Player
{
    f32 AngleToPoint(th095::Float3*);
};

struct ReplayManagerTaskView
{
    ReplayManagerTaskView * Create(int, char*);
    void Destroy();
};

struct ResultAnmVmDrawView
{
    i32 Draw();
};

struct SceneSelectColorInterpolationView
{
    void SetColor1Interpolation(int, unsigned char, unsigned int, unsigned int);
};

struct ScorePhotoStageView
{
    i32 CapturePhotoPixels(int);
};

struct ScreenEffectTimer
{
    int Tick();
};

struct SupervisorControllerView
{
    u16 GetInput(int);
};

struct SupervisorInputWorkerView
{
    void Start(void (*)(void*), void*);
    void Stop();
};

namespace EclExtended
{
struct AnmManagerLookupView
{
    ZunBool ExecuteScript(th095::AnmVm*);
    AnmVm * GetVm(int);
};
} // namespace EclExtended

namespace EclExtended
{
struct ExtendedAnmSpawner
{
    AnmVmId CreateVmAtWorld(int, th095::Float3*);
    void CreateVmAtWorldInto(th095::EclExtended::ExtendedVmHandle*, int, th095::Float3*);
    void InitializeVm(th095::AnmVm*, int);
};
} // namespace EclExtended

namespace EclExtended
{
struct ExtendedPhotoCameraView
{
    i32 CountPhotoTargets(float*, float*);
};
} // namespace EclExtended

namespace EclExtended
{
struct ExtendedPhotoEffectManager
{
    i32 Spawn(int, void*);
};
} // namespace EclExtended

namespace EclExtended
{
struct ExtendedPhotoEnemyManagerView
{
    i32 Spawn(int, th095::Float3 const*, int, int, int, unsigned int);
};
} // namespace EclExtended

namespace EclExtended
{
struct ExtendedVector
{
    void FromAngleMagnitude(float, float);
};
} // namespace EclExtended

namespace EclExtended
{
struct ExtendedVmHandle
{
    AnmVm * GetVm();
    ZunResult SetSprite(int);
};
} // namespace EclExtended

namespace EclRunHigh
{
struct AnmManagerLookup
{
    void ConfigureEnemyPhotoAnm(th095::AnmVm*, void*, int);
    AnmVm * FindVm(int);
    void RemoveVm(int);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct EnemyFloatOperandView
{
    f32 ResolveFloat(th095::EclRawOperand);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct PhotoAnmSpawner
{
    i32 Spawn(int, th095::Float3*);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct PhotoCamera
{
    f32 GetAngle(th095::Float3*);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct PhotoEffectManager
{
    i32 Spawn(int, void*);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct PhotoModeController
{
    void Begin();
    void End();
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct PhotoSession
{
    void Finish();
    void ReplaceActive();
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct PhotoSessionDescriptor
{
    PhotoSessionDescriptor * Create();
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct Th095BulletManager
{
    void ResetEnemyPatterns();
    void SpawnEnemyPattern(short*);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct Th095RuntimeManager
{
    void ResetEnemies();
    ZunResult SpawnEnemy(int, th095::Float3*, int, int, int, int*);
};
} // namespace EclRunHigh

namespace EclRunHigh
{
struct Th095StageController
{
    void ResetEnemyState();
};
} // namespace EclRunHigh

// --- class method definitions (TODO: real behaviour) ---
// TODO(port): th095::AnmManager::RemoveVmListNode(th095::AnmVmListNode*)
i32 AnmManager::RemoveVmListNode(th095::AnmVmListNode*) { /* TODO(port) */ return 0; }

// TODO(port): th095::EclExtended::AnmManagerLookupView::ExecuteScript(th095::AnmVm*)
namespace EclExtended
{
ZunBool AnmManagerLookupView::ExecuteScript(th095::AnmVm*) { /* TODO(port) */ return 0; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::AnmManagerLookupView::GetVm(int)
namespace EclExtended
{
AnmVm * AnmManagerLookupView::GetVm(int) { /* TODO(port) */ return NULL; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedAnmSpawner::CreateVmAtWorld(int, th095::Float3*)
namespace EclExtended
{
AnmVmId ExtendedAnmSpawner::CreateVmAtWorld(int, th095::Float3*) { /* TODO(port) */ return AnmVmId(); }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedAnmSpawner::CreateVmAtWorldInto(th095::EclExtended::ExtendedVmHandle*, int, th095::Float3*)
namespace EclExtended
{
void ExtendedAnmSpawner::CreateVmAtWorldInto(th095::EclExtended::ExtendedVmHandle*, int, th095::Float3*) { /* TODO(port) */ }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedAnmSpawner::InitializeVm(th095::AnmVm*, int)
namespace EclExtended
{
void ExtendedAnmSpawner::InitializeVm(th095::AnmVm*, int) { /* TODO(port) */ }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedPhotoCameraView::CountPhotoTargets(float*, float*)
namespace EclExtended
{
i32 ExtendedPhotoCameraView::CountPhotoTargets(float*, float*) { /* TODO(port) */ return 0; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedPhotoEffectManager::Spawn(int, void*)
namespace EclExtended
{
i32 ExtendedPhotoEffectManager::Spawn(int, void*) { /* TODO(port) */ return 0; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedPhotoEnemyManagerView::Spawn(int, th095::Float3 const*, int, int, int, unsigned int)
namespace EclExtended
{
i32 ExtendedPhotoEnemyManagerView::Spawn(int, th095::Float3 const*, int, int, int, unsigned int) { /* TODO(port) */ return 0; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedVector::FromAngleMagnitude(float, float)
namespace EclExtended
{
void ExtendedVector::FromAngleMagnitude(float, float) { /* TODO(port) */ }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedVmHandle::GetVm()
namespace EclExtended
{
AnmVm * ExtendedVmHandle::GetVm() { /* TODO(port) */ return NULL; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::ExtendedVmHandle::SetSprite(int)
namespace EclExtended
{
ZunResult ExtendedVmHandle::SetSprite(int) { /* TODO(port) */ return ZUN_SUCCESS; }
} // namespace EclExtended

// TODO(port): th095::EclFloatOperandPlayerView::AngleFromPoint(th095::Float3*)
f32 EclFloatOperandPlayerView::AngleFromPoint(th095::Float3*) { /* TODO(port) */ return 0.0f; }

// TODO(port): th095::EclManager::CallEclSub(th095::EnemyEclContext*, short)
ZunResult EclManager::CallEclSub(th095::EnemyEclContext*, short) { /* TODO(port) */ return ZUN_SUCCESS; }

// TODO(port): th095::EclOperandPlayerView::AngleFromPoint(th095::Float3*)
f32 EclOperandPlayerView::AngleFromPoint(th095::Float3*) { /* TODO(port) */ return 0.0f; }

// TODO(port): th095::EclRunHigh::AnmManagerLookup::ConfigureEnemyPhotoAnm(th095::AnmVm*, void*, int)
namespace EclRunHigh
{
void AnmManagerLookup::ConfigureEnemyPhotoAnm(th095::AnmVm*, void*, int) { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::AnmManagerLookup::FindVm(int)
namespace EclRunHigh
{
AnmVm * AnmManagerLookup::FindVm(int) { /* TODO(port) */ return NULL; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::AnmManagerLookup::RemoveVm(int)
namespace EclRunHigh
{
void AnmManagerLookup::RemoveVm(int) { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::EnemyFloatOperandView::ResolveFloat(th095::EclRawOperand)
namespace EclRunHigh
{
f32 EnemyFloatOperandView::ResolveFloat(th095::EclRawOperand) { /* TODO(port) */ return 0.0f; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoAnmSpawner::Spawn(int, th095::Float3*)
namespace EclRunHigh
{
i32 PhotoAnmSpawner::Spawn(int, th095::Float3*) { /* TODO(port) */ return 0; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoCamera::GetAngle(th095::Float3*)
namespace EclRunHigh
{
f32 PhotoCamera::GetAngle(th095::Float3*) { /* TODO(port) */ return 0.0f; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoEffectManager::Spawn(int, void*)
namespace EclRunHigh
{
i32 PhotoEffectManager::Spawn(int, void*) { /* TODO(port) */ return 0; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoModeController::Begin()
namespace EclRunHigh
{
void PhotoModeController::Begin() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoModeController::End()
namespace EclRunHigh
{
void PhotoModeController::End() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoSession::Finish()
namespace EclRunHigh
{
void PhotoSession::Finish() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoSession::ReplaceActive()
namespace EclRunHigh
{
void PhotoSession::ReplaceActive() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::PhotoSessionDescriptor::Create()
namespace EclRunHigh
{
PhotoSessionDescriptor * PhotoSessionDescriptor::Create() { /* TODO(port) */ return NULL; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::Th095BulletManager::ResetEnemyPatterns()
namespace EclRunHigh
{
void Th095BulletManager::ResetEnemyPatterns() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::Th095BulletManager::SpawnEnemyPattern(short*)
namespace EclRunHigh
{
void Th095BulletManager::SpawnEnemyPattern(short*) { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::Th095RuntimeManager::ResetEnemies()
namespace EclRunHigh
{
void Th095RuntimeManager::ResetEnemies() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::Th095RuntimeManager::SpawnEnemy(int, th095::Float3*, int, int, int, int*)
namespace EclRunHigh
{
ZunResult Th095RuntimeManager::SpawnEnemy(int, th095::Float3*, int, int, int, int*) { /* TODO(port) */ return ZUN_SUCCESS; }
} // namespace EclRunHigh

// TODO(port): th095::EclRunHigh::Th095StageController::ResetEnemyState()
namespace EclRunHigh
{
void Th095StageController::ResetEnemyState() { /* TODO(port) */ }
} // namespace EclRunHigh

// TODO(port): th095::Enemy::ClampPosition()
void Enemy::ClampPosition() { /* TODO(port) */ }

// TODO(port): th095::EnemyShotBulletManagerView::SpawnBulletPattern(th095::EnemyShotDescriptorView*)
i32 EnemyShotBulletManagerView::SpawnBulletPattern(th095::EnemyShotDescriptorView*) { /* TODO(port) */ return 0; }

// TODO(port): th095::FrontEndControllerView::Create(int)
FrontEndControllerView * FrontEndControllerView::Create(int) { /* TODO(port) */ return NULL; }

// TODO(port): th095::FrontEndControllerView::Destroy()
void FrontEndControllerView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::FrontEndLifecycleView::OnDraw(void*)
void FrontEndLifecycleView::OnDraw(void*) { /* TODO(port) */ }

// TODO(port): th095::FrontEndLifecycleView::OnUpdate(void*)
i32 FrontEndLifecycleView::OnUpdate(void*) { /* TODO(port) */ return 0; }

// TODO(port): th095::FrontEndPointerQueueView::Pop()
i32 FrontEndPointerQueueView::Pop() { /* TODO(port) */ return 0; }

// TODO(port): th095::HelpTextureEntryView::Clear()
void HelpTextureEntryView::Clear() { /* TODO(port) */ }

// TODO(port): th095::MidiOutput::ReadFileData(int, char*)
i32 MidiOutput::ReadFileData(int, char*) { /* TODO(port) */ return 0; }

// TODO(port): th095::MusicRoomTextureEntryView::Clear()
void MusicRoomTextureEntryView::Clear() { /* TODO(port) */ }

// TODO(port): th095::PhotoAnmManagerView::FindVm(int)
AnmVm * PhotoAnmManagerView::FindVm(int) { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoAnmManagerView::RemoveVm(int)
void PhotoAnmManagerView::RemoveVm(int) { /* TODO(port) */ }

// TODO(port): th095::PhotoAnmManagerView::SetVmInterrupt(int, short)
void PhotoAnmManagerView::SetVmInterrupt(int, short) { /* TODO(port) */ }

// TODO(port): th095::PhotoAnmManagerView::SetVmPosition(int, th095::Float3 const*)
void PhotoAnmManagerView::SetVmPosition(int, th095::Float3 const*) { /* TODO(port) */ }

// TODO(port): th095::PhotoBackgroundManagerView::Create()
PhotoBackgroundManagerView * PhotoBackgroundManagerView::Create() { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoBackgroundManagerView::Destroy()
void PhotoBackgroundManagerView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::PhotoBulletManagerTaskView::Create()
PhotoBulletManagerTaskView * PhotoBulletManagerTaskView::Create() { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoBulletManagerTaskView::Destroy()
void PhotoBulletManagerTaskView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::PhotoBulletManagerView::BeginPhotoCapture(th095::Float3 const*, th095::Float3 const*)
void PhotoBulletManagerView::BeginPhotoCapture(th095::Float3 const*, th095::Float3 const*) { /* TODO(port) */ }

// TODO(port): th095::PhotoBulletManagerView::CapturePhotoTargets(th095::Float3 const*, th095::Float3 const*)
ZunResult PhotoBulletManagerView::CapturePhotoTargets(th095::Float3 const*, th095::Float3 const*) { /* TODO(port) */ return ZUN_SUCCESS; }

// TODO(port): th095::PhotoBulletManagerView::CountNearbyTargets(th095::Float3 const*, float)
i32 PhotoBulletManagerView::CountNearbyTargets(th095::Float3 const*, float) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoBulletPlayerView::AngleFromPoint(th095::PhotoBulletVector*)
f32 PhotoBulletPlayerView::AngleFromPoint(th095::PhotoBulletVector*) { /* TODO(port) */ return 0.0f; }

// TODO(port): th095::PhotoBulletPlayerView::CheckBulletCollision(th095::PhotoBulletVector*, th095::PhotoBulletVector*)
i32 PhotoBulletPlayerView::CheckBulletCollision(th095::PhotoBulletVector*, th095::PhotoBulletVector*) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoBulletVector::FromAngleMagnitude(float, float)
void PhotoBulletVector::FromAngleMagnitude(float, float) { /* TODO(port) */ }

// TODO(port): th095::PhotoCaptureParticleSpawnerView::Spawn(int, th095::Float3*, unsigned int)
i32 PhotoCaptureParticleSpawnerView::Spawn(int, th095::Float3*, unsigned int) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoEnemyEclManagerView::RunEcl(th095::PhotoEnemyView*)
i32 PhotoEnemyEclManagerView::RunEcl(th095::PhotoEnemyView*) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoEnemyManagerTaskView::Destroy()
void PhotoEnemyManagerTaskView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::PhotoEnemyManagerTaskView::LoadResources()
i32 PhotoEnemyManagerTaskView::LoadResources() { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoEnemyManagerTaskView::RestartPhotoTargetEcls()
void PhotoEnemyManagerTaskView::RestartPhotoTargetEcls() { /* TODO(port) */ }

// TODO(port): th095::PhotoEnemyManagerTaskView::Update()
i32 PhotoEnemyManagerTaskView::Update() { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoEnemyPlayerView::CheckBulletCollision(th095::Float3*, th095::Float3*)
i32 PhotoEnemyPlayerView::CheckBulletCollision(th095::Float3*, th095::Float3*) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoGameFileSystemView::CheckIfFileAlreadyExists(char const*)
BOOL PhotoGameFileSystemView::CheckIfFileAlreadyExists(char const*) { /* TODO(port) */ return FALSE; }

// TODO(port): th095::PhotoItemManagerTaskView::Create()
PhotoItemManagerTaskView * PhotoItemManagerTaskView::Create() { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoItemManagerTaskView::Destroy()
void PhotoItemManagerTaskView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::PhotoLaserManagerTaskView::Create()
PhotoLaserManagerTaskView * PhotoLaserManagerTaskView::Create() { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoLaserManagerTaskView::Destroy()
void PhotoLaserManagerTaskView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::PhotoPlayerManagerView::Create()
PhotoPlayerManagerView * PhotoPlayerManagerView::Create() { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoPlayerManagerView::Destroy()
void PhotoPlayerManagerView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::PhotoResetTargetView::ResetForPhotoTransition()
void PhotoResetTargetView::ResetForPhotoTransition() { /* TODO(port) */ }

// TODO(port): th095::PhotoRuntimeConfigView::Initialize()
void PhotoRuntimeConfigView::Initialize() { /* TODO(port) */ }

// TODO(port): th095::PhotoStageAnmManagerView::GetVm(int)
AnmVm * PhotoStageAnmManagerView::GetVm(int) { /* TODO(port) */ return NULL; }

// TODO(port): th095::PhotoStageAnmManagerView::MarkVmForDeletion(int)
void PhotoStageAnmManagerView::MarkVmForDeletion(int) { /* TODO(port) */ }

// TODO(port): th095::PhotoStageAnmManagerView::SetInterrupt(int, int)
void PhotoStageAnmManagerView::SetInterrupt(int, int) { /* TODO(port) */ }

// TODO(port): th095::PhotoStageAnmManagerView::SetPosition(int, th095::Float3*)
void PhotoStageAnmManagerView::SetPosition(int, th095::Float3*) { /* TODO(port) */ }

// TODO(port): th095::PhotoStageBulletManagerView::ClearCapturedBullets()
i32 PhotoStageBulletManagerView::ClearCapturedBullets() { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoStageControllerView::CountNearbyTargets(th095::Float3 const*, float)
i32 PhotoStageControllerView::CountNearbyTargets(th095::Float3 const*, float) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoStageControllerView::CountPhotoTargets(th095::Float3 const*, th095::Float3 const*)
i32 PhotoStageControllerView::CountPhotoTargets(th095::Float3 const*, th095::Float3 const*) { /* TODO(port) */ return 0; }

// TODO(port): th095::PhotoStageEffectManagerView::CommitCapturedObjects()
i32 PhotoStageEffectManagerView::CommitCapturedObjects() { /* TODO(port) */ return 0; }

// TODO(port): th095::Player::AngleToPoint(th095::Float3*)
f32 Player::AngleToPoint(th095::Float3*) { /* TODO(port) */ return 0.0f; }

// TODO(port): th095::ReplayManagerTaskView::Create(int, char*)
ReplayManagerTaskView * ReplayManagerTaskView::Create(int, char*) { /* TODO(port) */ return NULL; }

// TODO(port): th095::ReplayManagerTaskView::Destroy()
void ReplayManagerTaskView::Destroy() { /* TODO(port) */ }

// TODO(port): th095::ResultAnmVmDrawView::Draw()
i32 ResultAnmVmDrawView::Draw() { /* TODO(port) */ return 0; }

// TODO(port): th095::SceneSelectColorInterpolationView::SetColor1Interpolation(int, unsigned char, unsigned int, unsigned int)
void SceneSelectColorInterpolationView::SetColor1Interpolation(int, unsigned char, unsigned int, unsigned int) { /* TODO(port) */ }

// TODO(port): th095::ScorePhotoStageView::CapturePhotoPixels(int)
i32 ScorePhotoStageView::CapturePhotoPixels(int) { /* TODO(port) */ return 0; }

// TODO(port): th095::ScreenEffectTimer::Tick()
int ScreenEffectTimer::Tick() { /* TODO(port) */ return 0; }

// TODO(port): th095::SupervisorControllerView::GetInput(int)
u16 SupervisorControllerView::GetInput(int) { /* TODO(port) */ return 0; }

// TODO(port): th095::SupervisorInputWorkerView::Start(void (*)(void*), void*)
void SupervisorInputWorkerView::Start(void (*)(void*), void*) { /* TODO(port) */ }

// TODO(port): th095::SupervisorInputWorkerView::Stop()
void SupervisorInputWorkerView::Stop() { /* TODO(port) */ }

// --- constructor / destructor definitions ---
// TODO(port): zero-init of unknown fields once layout is confirmed
AnmVm::AnmVm() { }
AnmVm::~AnmVm() { }

// The real class (zwave.hpp, namespace th095) has exactly one member,
// LPDIRECTSOUND8 m_pDS, and no virtual functions — the object is 8 bytes.
// CSoundManager::Initialize runs SAFE_RELEASE(m_pDS) BEFORE
// DirectSoundCreate8 fills it in; with heap garbage there it dispatches
// a virtual call through an uninitialized vtable (Atmosphere crash
// 2168-0001: instruction abort in the data section on the
// SoundPlayer worker thread). Zero the member.
CSoundManager::CSoundManager() { memset(this, 0, sizeof(void *)); }

// TODO(port): zero-init of unknown fields once layout is confirmed
PhotoEnemyManagerTaskView::PhotoEnemyManagerTaskView() { }
PhotoEnemyManagerTaskView::~PhotoEnemyManagerTaskView() { }

// --- free function definitions (TODO: real behaviour) ---
// TODO(port): th095::CreateFrontEndGameManager(int)
FrontEndGameManagerView * CreateFrontEndGameManager(int) { /* TODO(port) */ return NULL; }

// TODO(port): th095::EclExtended::DispatchExtendedValue(int, int, int, int, int, int)
namespace EclExtended
{
void DispatchExtendedValue(int, int, int, int, int, int) { /* TODO(port) */ }
} // namespace EclExtended

// TODO(port): th095::EclExtended::GetPhotoBulletScriptBase(int)
namespace EclExtended
{
i32 GetPhotoBulletScriptBase(int) { /* TODO(port) */ return 0; }
} // namespace EclExtended

// TODO(port): th095::EclExtended::PhotoToScreen(th095::Float3*, th095::Float3 const*)
namespace EclExtended
{
ZunResult PhotoToScreen(th095::Float3*, th095::Float3 const*) { /* TODO(port) */ return ZUN_SUCCESS; }
} // namespace EclExtended

// TODO(port): th095::GetPhotoEffectScriptBase(int)
i32 GetPhotoEffectScriptBase(int) { /* TODO(port) */ return 0; }

// TODO(port): th095::PreparePhotoResultScreen(th095::ResultScreen*)
ZunResult PreparePhotoResultScreen(th095::ResultScreen*) { /* TODO(port) */ return ZUN_SUCCESS; }

// TODO(port): th095::ReleaseReplayAnm()
i32 ReleaseReplayAnm() { /* TODO(port) */ return 0; }

// TODO(port): th095::ReleaseSceneSelectAnms()
void ReleaseSceneSelectAnms() { /* TODO(port) */ }

// TODO(port): th095::RotatePhotoEffectVector(th095::Float3*, th095::Float3 const*, float)
ZunResult RotatePhotoEffectVector(th095::Float3*, th095::Float3 const*, float) { /* TODO(port) */ return ZUN_SUCCESS; }

// TODO(port): th095::RotatePhotoStagePoint(th095::Float3*, th095::Float3 const*, float)
ZunResult RotatePhotoStagePoint(th095::Float3*, th095::Float3 const*, float) { /* TODO(port) */ return ZUN_SUCCESS; }

// TODO(port): th095::SpawnPhotoStageEffect(int, int, int, unsigned int, int, int)
ZunResult SpawnPhotoStageEffect(int, int, int, unsigned int, int, int) { /* TODO(port) */ return ZUN_SUCCESS; }

} // namespace th095
