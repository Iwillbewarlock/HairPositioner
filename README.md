# HairPositioner

플레이어 헤어의 위치/회전/크기를 **레이스메뉴 슬라이더**로 조정하는 SKSE 플러그인.
모든 종족에서 동작한다.

- 대상: Skyrim SE / AE (VR 미지원)
- 범위: **플레이어 전용, 헤어(headpart slot 3)만**
- 의존성: SKSE64, Address Library, RaceMenu

---

## 동작 방식

헤어 메시의 **정점 좌표를 직접 다시 써서** 위치·회전·크기를 바꾼다. 노드 트랜스폼이나
앵커 본을 쓰지 않으므로 HDT-SMP, 참수, 애니메이션과 충돌하지 않는다.

```
변환   v' = S ⊙ (R·(v − pivot) + T) + pivot
회전   R = Ry(y)·Rz(z)·Rx(x), 각도는 도(degree)
피벗   object(1) = 원본 정점 전체의 평균
       space(2)  = (0,0,0)
       bone(0)   = 정점을 가장 많이 가진 본의 −skinToBone.translate   (기본)
저장   SKSE 코세이브, 액터별·슬롯별 (헤어별이 아님)
```

정점 쓰기 경로는 두 가지다. 어느 쪽인지는 RTTI 가 아니라 `vertexDesc` 의 동적 정점 크기
니블로 정한다 — RaceMenu 가 헤드파츠를 `BSDynamicTriShape` 로 바꿔도 그 니블이 0 이면
위치는 여전히 스킨 파티션에 있다.

- **동적 버퍼**: `BSDynamicTriShape::dynamicData` 에 스핀락 걸고 xyz 를 직접 쓴다.
  `StripMorphData = true`(기본)면 "FOD" FaceGen 베이스 데이터를 떼어내서 엔진이 원본으로
  되돌리지 못하게 한다.
- **스킨 파티션**: `NiSkinPartition` 을 딥카피해서 위치를 float4 로 다시 쓰고
  (`VF_FULLPREC` 는 신뢰하지 않는다 — 런타임 파티션에서는 그 플래그가 꺼져 있어도 float 이다)
  `UpdateSubresource` 로 GPU 에 올린 뒤 스킨 인스턴스의 파티션을 교체한다.

원본 정점은 엔진이 소유한 상태에서만 캡처하고, 항상 원본에서 다시 계산하므로 오프셋이
누적되지 않는다.

**게임 함수 훅이 없다.** 엔진 이벤트(3D 로드, 레이스메뉴 열림/닫힘, 장비 착용)와 하트비트가
"머리가 다시 만들어졌는가"를 확인하고 재적용한다. 하트비트는 평소 500ms, 레이스메뉴가 열려
있거나 장비를 바꾼 직후 3초 동안은 33ms 라서 메뉴 안에서 헤어를 바꿔도 사실상 다음 프레임에
다시 적용된다. 틱은 장면을 순회하지 않는다 — 얼굴 노드·가발 루트 포인터와 우리가 쓴 파티션
포인터만 비교하고, 그게 바뀌었을 때만 순회한다. 정점 쓰기도 중간 버퍼 없이 변환 결과를 메시에
바로 쓴다.

**가발 슬롯**(`WigSlots = 31,41`): 해당 슬롯에 착용한 장비의 지오메트리 전부에 같은 변환·같은
피벗을 적용한다. 단 **기본은 꺼짐** — 31/41 은 투구·후드도 쓰는 슬롯이라 켜 두면 헬멧이 같이
움직인다. RaceMenu 의 "Hair Move Wig/Helmet Too" 슬라이더(0/1)로 켜며, 값은 다른 슬라이더와
같이 프리셋·코세이브에 저장된다. 끄는 순간 그때까지 옮겨 둔 착용 메시는 원위치로 돌려놓는다.

**프리셋**: 슬라이더 값을 RaceMenu 의 body-morph 저장소(`NiOverride.SetBodyMorph`, 키
`HairPositioner`)에 미러링한다. RaceMenu 가 그 저장소를 자기 코세이브와 캐릭터 프리셋(.jslot)에 넣고
불러올 때 복원하므로, RaceMenu 의 원래 Save/Load Preset 버튼만으로 헤어 오프셋이 따라간다. 별도
프리셋 파일·슬라이더는 없다. 미러는 헤어에 의미가 없어 지원하지 않는다.

---

## 빌드 순서

### 1. `setup.bat`
CommonLibSSE-NG 를 `extern/` 에 클론한다. 한 번만.

### 2. `build.bat`
DLL 을 빌드하고 `package\SKSE\Plugins\HairPositioner.dll` 로 복사한다.
`VCPKG_ROOT` 환경변수가 필요하다.

### 3. `compile.bat`
파피루스 스크립트 두 개를 컴파일해서 `package\Scripts\` 에 넣는다.
처음 실행할 때 CK 의 `Scripts.zip` 을 `papyrus\import\vanilla` 로 자동 압축 해제한다.

경로는 `compile.bat` 위쪽에 상수로 박혀 있다. 다르면 그 줄만 고치면 된다.

```
PapyrusCompiler : D:\TAKEALOOK\Stock Game\Papyrus Compiler\PapyrusCompiler.exe
Creation Kit    : D:\TAKEALOOK\mods\Creation Kit\Root
SKSE 소스       : D:\TAKEALOOK\mods\Skyrim Script Extender (SKSE64)\Scripts\Source
```

세 배치 파일 모두 로그를 남긴다 (`cmake_log_latest.txt`, `build_log_latest.txt`,
`compile.log`).

---

## 배포 패키지

`package\` 폴더를 그대로 MO2 모드로 만들면 된다.

```
package\
  HairPositioner.esp                      ESL 플래그, RaceMenu 플러그인 퀘스트
  SKSE\Plugins\HairPositioner.dll
  Scripts\RaceMenuHairPositioner.pex
  Scripts\HairPositioner.pex
  Scripts\Source\*.psc
  Interface\translations\HairPositioner_*.txt
```

`HairPositioner.esp` 는 여느 RaceMenu 슬라이더 플러그인과 같은 구조의 최소 플러그인이다. 퀘스트 하나 + 플레이어 별칭 하나뿐이고, 별칭에는 RaceMenu 의
`RaceMenuLoad` 스크립트가 붙어 있다. 레코드를 덮어쓰지 않으므로 로드 오더 위치를
가리지 않는다.

---

## 사용

레이스메뉴(`showracemenu`) **Hair** 탭에 아래 슬라이더가 붙는다.

| 슬라이더 | 범위 |
|---|---|
| 헤어 위치 X · Y · Z (좌우 / 앞뒤 / 상하) | -15 ~ 15, 0.01 |
| 헤어 회전 X · Y · Z | -90 ~ 90 도, 0.1 |
| 헤어 크기 X · Y · Z | 0.5 ~ 2.0 |
| 헤어 기준점 | 0 본(기본) / 1 중심 / 2 원점 |
| 헤어 초기화 | 1 로 올리면 초기화 (자동으로 0 복귀) |

값은 **세이브(코세이브)에 액터별로** 저장된다. 헤어를 바꿔도 같은 조정값이 새 헤어에
적용되고, 다른 캐릭터 세이브에는 영향이 없다.

RaceMenu 프리셋 연동: 슬라이더 값은 RaceMenu 의 body-morph 저장소(`NiOverride.SetBodyMorph`,
키 `HairPositioner`)에도 같이 기록된다. RaceMenu 는 이 저장소를 자기 코세이브와 캐릭터 프리셋(.jslot)에
넣고 불러올 때 복원하므로, **RaceMenu 의 원래 Save Preset / Load Preset 버튼만으로** 헤어 오프셋이
같이 저장되고 같이 불러와진다. 프리셋을 불러오면 RaceMenu 가 메뉴를 다시 초기화하고, 그때 저장소 값을
읽어 플러그인에 넣는다. 훅 없음, RaceMenu 공개 API 만 사용. (morph 이름은 어떤 메시에도 없어서 몸에는
아무 영향이 없다.)

설정: `Data\SKSE\Plugins\HairPositioner.ini` — `StripMorphData`, `WigSlots`.

### 콘솔 (디버그용)

플러그인은 **게임 함수 훅을 하나도 쓰지 않는다.** 콘솔 명령은 바닐라 `cgf`
(CallGlobalFunction) 로 파피루스 네이티브를 직접 부르는 방식이다.

```
cgf "HairPositioner.Probe"              현재 상태 / 지오메트리 진단 (콘솔 + 로그)
cgf "HairPositioner.Show"               현재 키와 값 출력
cgf "HairPositioner.SetChannel" 2 5.0   헤어 위로 5 (채널: 0-2 이동, 3-5 회전, 6-8 크기)
cgf "HairPositioner.Reset"
```

로그: `Documents\My Games\Skyrim Special Edition\SKSE\HairPositioner.log`

---

## 문제가 생기면

1. 세이브 로드 → 로그에 `heartbeat started` 와 `sink:` 줄 확인
2. 콘솔에 `cgf "HairPositioner.Probe"` — 머리 노드 밑 지오메트리와 매칭 결과가 출력된다
3. `showracemenu` → Hair 탭 아래쪽 슬라이더

로그: `Documents\My Games\Skyrim Special Edition\SKSE\HairPositioner.log`
