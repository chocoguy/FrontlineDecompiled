// CSoldierObject: an animated soldier (enemies and allies; the player
// object reuses parts of it). Hand-maintained.
#ifndef _GAME_SOLDIER_OBJECT_H_
#define _GAME_SOLDIER_OBJECT_H_

#include "types.h"
#include "game_types.h"
#include "game/ISceneNode.h"
#include "game/anim_obj.h"
#include "game/AISoldierDoodad.h"
#include "game/animmorph.h"
#include "game/shadow.h"
#include "game/jaw_controller.h"
#include "game/sphere.h"
#include "string.h"

class CAIDoodad;
class CBullet;
class CCollision;
class CDrawContext;
class CLight;
class CMatrix;
class CStaticMesh;
class CStaticObject;
class CTexture;
class CVector3;
class CWeapon;

// Sound settings a soldier's script keeps for itself (0x20 bytes, copied
// field by field by Get/SetMySoundInfoStruct).
struct CSoldierSoundInfo {
    /* 0x0 */ s32 field_0x0;
    /* 0x4 */ s32 field_0x4;
    /* 0x8 */ s32 field_0x8;
    /* 0xC */ s32 field_0xC;
    /* 0x10 */ f32 field_0x10;
    /* 0x14 */ s32 field_0x14;
    /* 0x18 */ s32 field_0x18;
    /* 0x1C */ s32 field_0x1C;
};

// What sits on an attach point (m_attachType).
enum {
    SOLDIER_ATTACH_NONE = 0,
    SOLDIER_ATTACH_OBJECT = 1,  // a CStaticObject
    SOLDIER_ATTACH_MESH = 2,    // a bare CStaticMesh
};

enum { SOLDIER_NUM_WEAPONS = 9, SOLDIER_NUM_ATTACH = 8 };

// size 0x44B0; vtable 0x801882F0
// Weapons, objects attached to the skeleton's attach points (hands, head),
// a morph-target face (expressions, blinking, jaw), head tracking, bullet
// hit reactions and fading out after death.
class CSoldierObject : public CAnimObject {
public:
    virtual void InitAI();  // 0x800AE218

    /* 0x30E0 */ CAISoldierDoodad m_aiDoodad;
    /* 0x3134 */ CTexture* m_pBodyTexture;
    /* 0x3138 */ CTexture* m_pFaceTexture;
    /* 0x313C */ s8 m_bodyTexturePart;  // < 0: all parts
    /* 0x313D */ u8 unk_0x313D[0x3];
    /* 0x3140 */ CSoldierSoundInfo m_soundInfo;  // scripts read/write it whole
    /* 0x3160 */ u8 m_noNonVisDestruct;  // keep the soldier when out of view
    /* 0x3161 */ u8 field_0x3161;
    /* 0x3162 */ u8 unk_0x3162[0xE];
    /* 0x3170 */ CAnimMorph m_face;
    /* 0x3F60 */ CMatrix m_upperTM;  // upper body (AI eyes): m_tm moved by m_headPosition
    /* 0x3FA0 */ f32 m_headPosition[4];  // local offset of m_upperTM (AI: m_headPosition)
    /* 0x3FB0 */ f32 m_lookLimits[4];  // head tracking pitch and yaw ranges
    /* 0x3FC0 */ u8 m_floatingBoneOn;
    /* 0x3FC1 */ u8 unk_0x3FC1[0x3];
    /* 0x3FC4 */ CStaticObject* m_pFloatingObject;  // follows the animation's floating bone
    /* 0x3FC8 */ u8 unk_0x3FC8[0x8];
    /* 0x3FD0 */ CMatrix m_floatingTM;
    /* 0x4010 */ CWeapon* m_pWeapons[SOLDIER_NUM_WEAPONS];
    /* 0x4034 */ void* m_attached[SOLDIER_NUM_ATTACH];  // per attach point
    /* 0x4054 */ u32 m_attachType[SOLDIER_NUM_ATTACH];  // SOLDIER_ATTACH_*
    /* 0x4074 */ int m_curWeapon;  // index into m_pWeapons
    /* 0x4078 */ u8 unk_0x4078[0x8];
    /* 0x4080 */ CMatrix m_attachTM[SOLDIER_NUM_ATTACH];  // attached object relative to its point
    /* 0x4280 */ u8 m_hasEmitter;  // bullets leave from a hard point of an attached gun
    /* 0x4281 */ u8 unk_0x4281[0x3];
    /* 0x4284 */ u32 m_emitterPoint;  // attach point holding the gun
    /* 0x4288 */ u8 unk_0x4288[0x8];
    /* 0x4290 */ CMatrix m_emitterTM;  // the muzzle hard point
    /* 0x42D0 */ s32 m_moodTarget;  // morph target held at full weight (-1 none)
    // Facial expression: the current one fades out while the next fades in.
    /* 0x42D4 */ s32 m_expression;  // -1 none
    /* 0x42D8 */ s32 m_nextExpression;  // -1 none
    /* 0x42DC */ f32 m_expressionWeight;
    /* 0x42E0 */ f32 m_nextExpressionWeight;
    /* 0x42E4 */ f32 m_expressionTarget;
    /* 0x42E8 */ f32 m_nextExpressionTarget;
    /* 0x42EC */ f32 m_expressionRate;
    /* 0x42F0 */ f32 m_nextExpressionRate;
    // Blinking: random intervals between m_blinkMin and m_blinkMax. The
    // timer counts down through closing, closed and opening.
    /* 0x42F4 */ f32 m_blinkTimer;
    /* 0x42F8 */ f32 m_blinkMin;
    /* 0x42FC */ f32 m_blinkMax;
    /* 0x4300 */ f32 m_blinkClose;
    /* 0x4304 */ f32 m_blinkHold;
    /* 0x4308 */ f32 m_blinkOpen;
    /* 0x430C */ u8 unk_0x430C[0x4];
    /* 0x4310 */ f32 m_blinkOpenEnd;    // timer values where the phases change
    /* 0x4314 */ f32 m_blinkClosedEnd;
    /* 0x4318 */ f32 m_blinkCloseEnd;
    /* 0x431C */ u8 m_faceStateQueued;
    /* 0x431D */ u8 unk_0x431D[0x1];
    /* 0x431E */ u16 m_faceState;
    /* 0x4320 */ CVolSphere m_localSphere1;  // EVolumeType 1
    /* 0x4340 */ CVolSphere m_worldSphere1;
    /* 0x4360 */ CVolSphere m_localSphere2;  // EVolumeType 2
    /* 0x4380 */ CVolSphere m_worldSphere2;
    /* 0x43A0 */ int m_soundHandle;  // the tracked SoundSchedule sound (-1: none)
    /* 0x43A4 */ u32 field_0x43A4;
    /* 0x43A8 */ int m_helmetChance;  // percent of head shots a helmet takes
    /* 0x43AC */ f32 m_helmetHitScale;  // damage of a head shot a helmet stopped
    /* 0x43B0 */ u8 m_fading;
    /* 0x43B1 */ u8 unk_0x43B1[0x3];
    /* 0x43B4 */ f32 m_fadeTime;
    /* 0x43B8 */ f32 m_fadeDuration;
    /* 0x43BC */ f32 field_0x43BC;
    /* 0x43C0 */ u8 m_fadeDropped;  // weapons dropped while fading
    /* 0x43C1 */ u8 m_headTrackPlayer;
    /* 0x43C2 */ u8 unk_0x43C2[0x2];
    /* 0x43C4 */ int m_headTrackSteps;  // frames to turn the head to (or from) the player
    /* 0x43C8 */ int m_headTrackStep;
    /* 0x43CC */ u8 unk_0x43CC[0x4];
    /* 0x43D0 */ CCharacterShadow m_shadow;
    /* 0x4440 */ CJawController m_jaw;
    /* 0x4480 */ s32 m_defaultExpression;
    /* 0x4484 */ u8 m_mmgFired;  // this frame: draw m_pMMGWeapon's flash on m_pMMG
    /* 0x4485 */ u8 unk_0x4485[0x3];
    /* 0x4488 */ CStaticObject* m_pMMG;
    /* 0x448C */ CWeapon* m_pMMGWeapon;  // fires mounted machine guns
    /* 0x4490 */ f32 m_burstTimer;  // hits within it count as one burst
    /* 0x4494 */ int m_burstHits;
    // Head translation: the root follows the vision cone joint (0 off,
    // 1 starting, 2 on, 3 stopping).
    /* 0x4498 */ s8 m_headTrans;
    /* 0x4499 */ u8 unk_0x4499[0x7];
    /* 0x44A0 */ CVector3 m_headTransOffset;

    void SetMood(EMoodType);  // 0x800244E0

    // Fades to expression `expr` at `weight` (immediately if nothing else
    // is fading).
    void SetExpression(int expr, float weight) {
        if (m_nextExpression >= 0) {
            m_expressionTarget = weight;
            m_expressionRate = 0.0f;
            m_expressionWeight = weight;
            m_expression = expr;
            m_nextExpressionTarget = 0.0f;
            m_nextExpressionRate = 0.0f;
            m_nextExpressionWeight = 0.0f;
            m_nextExpression = -1;
        } else if (m_expression == -1) {
            m_expressionTarget = weight;
            m_expressionRate = 0.25f * m_expressionTarget;
            m_expressionWeight = 0.0f;
            m_expression = expr;
        } else if (m_expression != expr) {
            m_expressionRate = 0.25f * -m_expressionWeight;
            m_nextExpressionTarget = weight;
            m_nextExpressionRate = 0.25f * m_nextExpressionTarget;
            m_nextExpressionWeight = 0.0f;
            m_nextExpression = expr;
        }
    }

    void SetBlink(float min, float max, float close, float open, float hold) {
        m_blinkMin = min;
        m_blinkMax = max;
        m_blinkClose = close;
        m_blinkOpen = open;
        m_blinkHold = hold;
    }

    CJawController* GetJaw() { return &m_jaw; }

    // The skeleton attach point called `name`, or -1.
    u32 FindAttachPoint(const char* name) {
        CharSkel_t* skel = m_anim.m_pSkel;
        for (u32 i = 0; i < skel->numMatrices; i++) {
            if (stricmp(skel->matrices[i].name, name) == 0) {
                return i;
            }
        }
        return -1;
    }

    // An attach point's world matrix: its joint, then its own offset.
    void GetAttachPointTM(CMatrix& out, u32 point) {
        out.Multiply(m_bones[m_anim.m_pSkel->matrices[point].joint], m_tm);
        out.Multiply(m_attachTM[point], out);
    }

    virtual CAIDoodad* GetAIDoodad();  // 0x80032B00
    virtual const CAIDoodad* GetAIDoodad() const { return &m_aiDoodad; }  // 0x800A9B2C (weak)
    virtual const CSoldierObject* AsSoldierObject() const { return this; }  // 0x800A9B34 (weak)
    virtual CSoldierObject* AsSoldierObject() { return this; }  // 0x800A9B38 (weak)
    virtual void Destroy();  // 0x800A9B3C
    void SetHeadTrans(bool);  // 0x800A9B80
    void SetHeadTrackPlayer(bool, int);  // 0x800A9BB4
    void FadeOut(float);  // 0x800A9BD8
    void SetBulletEmitter(CStaticObject*, char*);  // 0x800A9C04
    virtual void DetachFloatingBone();  // 0x800A9D04
    virtual void AttachFloatingBone();  // 0x800A9D1C
    void DetachObjectFromFloatingBone();  // 0x800A9D34
    int AttachObjectToFloatingBone(CStaticObject*, char*, int);  // 0x800A9D44
    virtual int GetAttachPointMatrix(char*, CMatrix*);  // 0x800A9E84
    void SetHelmetOffset(CVector3&);  // 0x800A9F1C
    int AttachMesh(CStaticMesh*, char*, int);  // 0x800AA01C
    virtual CStaticObject* DetachObject(char*);  // 0x800AA1B8
    virtual bool AttachObject(CStaticObject*, char*, int);  // 0x800AA25C
    bool FireMMG(CVector3&, CStaticObject*);  // 0x800AA40C
    bool FireWeapon(int, CVector3&, CVector3&, float);  // 0x800AA720
    void GetBulletEmitterMatrix(CMatrix&);  // 0x800AABB8
    int EjectHelmet(CVector3*);  // 0x800AAC78
    virtual void HandleBulletCollision(CBullet*, const CCollision&);  // 0x800AAEA0
    void GetLookMatrix(CMatrix&);  // 0x800ABA4C
    int AddWeapon(int, int);  // 0x800ABB1C
    virtual void SetAttachedLight(CLight*, CVector3);  // 0x800ABBBC
    virtual void OnCollision(const CCollision&);  // 0x800ABC6C
    virtual const IVolume* GetWorldBoundingVolume(ISceneNode::EVolumeType) const;  // 0x800AC310
    virtual const IVolume* GetLocalBoundingVolume(ISceneNode::EVolumeType) const;  // 0x800AC368
    virtual void UpdateLocalBoundingVolume(ISceneNode::EVolumeType, const CVector3&, const CVector3&);  // 0x800AC3C0
    virtual int CommitUpdate();  // 0x800AC428
    virtual int AttemptUpdate(float);  // 0x800AC448
    virtual void UpdateAI(float);  // 0x800AC4AC
    virtual int BeginUpdate(float);  // 0x800AC648
    void DrawMuzzleFlash(CDrawContext&, CWeapon*, bool);  // 0x800AC730
    virtual void Draw(CDrawContext&);  // 0x800AC9E4
    virtual void UpdateAnimation(float);  // 0x800ACFB8
    int SetFaceAnimationState(unsigned short, long);  // 0x800ADEAC
    void RemoveWeapons();  // 0x800ADEC0
    void CleanUp();  // 0x800ADFA4
    virtual ~CSoldierObject();  // 0x800AE160
    virtual int Init();  // 0x800AE240
    CSoldierObject();  // 0x800AE72C
};

SIZE_ASSERT(CSoldierObject, 0x44B0);
OFFSET_ASSERT(CSoldierObject, m_aiDoodad, 0x30E0);
OFFSET_ASSERT(CSoldierObject, m_soundInfo, 0x3140);
OFFSET_ASSERT(CSoldierObject, m_face, 0x3170);
OFFSET_ASSERT(CSoldierObject, m_upperTM, 0x3F60);
OFFSET_ASSERT(CSoldierObject, m_floatingTM, 0x3FD0);
OFFSET_ASSERT(CSoldierObject, m_pWeapons, 0x4010);
OFFSET_ASSERT(CSoldierObject, m_curWeapon, 0x4074);
OFFSET_ASSERT(CSoldierObject, m_attachTM, 0x4080);
OFFSET_ASSERT(CSoldierObject, m_emitterTM, 0x4290);
OFFSET_ASSERT(CSoldierObject, m_moodTarget, 0x42D0);
OFFSET_ASSERT(CSoldierObject, m_faceState, 0x431E);
OFFSET_ASSERT(CSoldierObject, m_localSphere1, 0x4320);
OFFSET_ASSERT(CSoldierObject, m_soundHandle, 0x43A0);
OFFSET_ASSERT(CSoldierObject, m_shadow, 0x43D0);
OFFSET_ASSERT(CSoldierObject, m_jaw, 0x4440);
OFFSET_ASSERT(CSoldierObject, m_defaultExpression, 0x4480);
OFFSET_ASSERT(CSoldierObject, m_headTrans, 0x4498);
OFFSET_ASSERT(CSoldierObject, m_headTransOffset, 0x44A0);

#endif
